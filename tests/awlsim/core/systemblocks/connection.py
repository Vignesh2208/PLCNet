
from awlsim.core.systemblocks.connection_params import *
import math
import socket
import select
import sys
#import queue
#from queue import *
import multiprocessing, Queue
#from multiprocessing import Queue as Queue
from multiprocessing import Array as Array
import time
import os

import set_connection
from datetime import datetime



import threading
from threading import *
from awlsim.core.systemblocks.modbus_slave import *
from awlsim.core.systemblocks.modbus_master import *
from awlsim.common.compat import *
from awlsim.core.datastructure import *
from awlsim.core.datatypes import ByteArray

from awlsim.core.systemblocks.systemblocks import *
from awlsim.core.util import *
from awlsim.definitions import *

from multiprocessing import Process, Condition, Lock  
from multiprocessing.managers import BaseManager  
import awlsim.core.systemblocks.test_run
from awlsim.core.systemblocks.test_run import test_run_server_ip
from awlsim.core.systemblocks.test_run import test_run_client_ip
from awlsim.core.systemblocks.test_run import test_run_client_serial
from awlsim.core.systemblocks.test_run import test_run_server_serial
from awlsim.core.systemblocks.test_run import put
from awlsim.core.systemblocks.test_run import get


import ctypes
libc = ctypes.CDLL('libc.so.6')


class Timespec(ctypes.Structure):
	""" timespec struct for nanosleep, see:
      	http://linux.die.net/man/2/nanosleep """
	_fields_ = [('tv_sec', ctypes.c_long),
	('tv_nsec', ctypes.c_long)]

libc.nanosleep.argtypes = [ctypes.POINTER(Timespec),
                           ctypes.POINTER(Timespec)]
nanosleep_req = Timespec()
nanosleep_rem = Timespec()

def nsleep(us):
	#print('nsleep: {0:.9f}'.format(us)) 
	""" Delay microseconds with libc nanosleep() using ctypes. """
	if (us >= 1000000):
		sec = us/1000000
		us %= 1000000
	else: sec = 0
	nanosleep_req.tv_sec = int(sec)
	nanosleep_req.tv_nsec = int(us * 1000)

	libc.nanosleep(nanosleep_req, nanosleep_rem)


NOT_STARTED = -1
RUNNING = 4
DONE = 0
CONN_TIMEOUT_ERROR = 1
RECV_TIMEOUT_ERROR = 2
SERVER_ERROR = 3
CLIENT_ERROR = 5

NO_ERROR = 0
ILLEGAL_FUNCTION = 1
ILLEGAL_DATA_ADDRESS = 2
ILLEGAL_DATA_VALUE = 3
SLAVE_DEVICE_FAILURE = 4

START_END_FLAG = 0x7E
ESCAPE_FLAG = 0x7D


modbus_area_data_types = {
		0 : "Unused",
		1 : "Coils",
		2 : "Inputs",
		3 : "Holding_Register",
		4 : "Input_Register",
	}	

class SharedQueue(object):
	def __init__(self):
		self.data = None
		self.data_available = 0

	def get(self,block=True):

		if block == False :
			if self.data_available == 1 :
				self.data_available = 0
				print("Got non blocking data from queue = ", self.data)
				return self.data
			else:
				return None
		else :
			while self.data_available == 0 :
				pass
			self.data_available = 0
			print("Got blocking data from queue = ", self.data)
			return self.data

	def put(self,data):
		print("Data put into queue = ", data)
		self.data = data
		self.data_available = 1





class Connection(object) :

	
	def __init__(self,cpu,connection_id,remote_port,local_port,remote_host_name,is_server,single_write_enabled,data_areas):

		self.connection_params = Connection_Params(cpu)
		self.IDS_IP = None
			
		no_error = False
		while no_error == False :
			try:
				self.thread_cmd_queue = Array('i',range(2000))
				self.thread_resp_queue = Array('i',range(2000))
				self.thread_resp_arr = Array('i', range(2000))
				self.thread_cmd_arr = Array('i', range(2000))
				no_error = True
			except :
				no_error = False


		
				

		self.connection_params.set_connection_params(connection_id,remote_port,local_port,self.hostname_to_ip(remote_host_name),is_server,single_write_enabled)
		self.cpu = cpu
		self.remote_host_id = int(remote_host_name)
		self.local_host_id = self.cpu.local_id
		
		self.data_area_dbs = {}
		self.STATUS = NOT_STARTED
		self.is_server = is_server
		#self.param_init_lock = threading.Lock()
		#self.status_lock = threading.Lock()
		self.param_init_lock = Lock()
		self.status_lock = Lock()
		self.PREV_ENQ_ENR = 0
		self.read_finish_status = 1

		# in/out parameters
		self.unit = 0
		self.data_type = -1
		self.start_address = -1
		self.length = -1
		self.TI = -1
		self.write_read = False

		# in parameters
		self.ENQ_ENR = 0		
		self.disconnect = True
		self.recv_time = 1.2 # 1.2 sec default
		self.conn_time = 5   # 5 sec default

		# out parameters
		self.ERROR = False
		self.STATUS_MODBUS = 0x0000
		self.STATUS_CONN = 0x0000
		self.STATUS_FUNC = "MODBUSPN"
		self.IDENT_CODE = "NONE"
		self.CONN_ESTABLISHED = False
		self.BUSY = False
		self.conf_dir = conf_directory

		self.server_process = None
		self.client_process = None
		

		if is_server == True :
			self.mod_server = ModBusSlave(self)
		else :
			self.mod_client = ModBusMaster(self)

		i = 1
		for area in data_areas :
			fields = area.split(',')
			fields = [int(entry) for entry in fields]
			if len(fields) != 4 :
				raise AwlSimError("Required number of fields not specified for Data Area in configuration file ")
			dataType = fields[0]
			dbNumber = fields[1]
			start = fields[2]
			end = fields[3]
			if dataType != 0 :
				self.connection_params.data_area[i].data_type = dataType
				self.connection_params.data_area[i].db = dbNumber
				self.connection_params.data_area[i].start = start
				self.connection_params.data_area[i].end = end

				if end < start :
					raise AwlSimError("End value cannot be less than start in configuration file ")

				if dataType == 1 or dataType == 2 : # coils
					numberOfWords = math.ceil(float(end - start)/16.0)
				else: 	
					numberOfWords = end - start + 1

				if dbNumber not in self.cpu.dbs.keys() :
					data_area_db = DB(dbNumber,codeBlock=None,permissions=(1 << 0 | 1 << 1))
					self.cpu.dbs[dbNumber] = data_area_db
					j = 1
					while j <= numberOfWords :
						curr_dataType = AwlDataType.makeByName("WORD")
						curr_field_name = modbus_area_data_types[dataType] + "_" + str(j)
						data_area_db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)
						j = j + 1

					data_area_db.allocate()
					j = 1
					while j <= numberOfWords :
						curr_field_name = modbus_area_data_types[dataType] + "_" + str(j)
						data_area_db.structInstance.setFieldDataByName(curr_field_name, 0)
						j = j + 1
					self.data_area_dbs[i] = data_area_db
				else :
					raise AwlSimError("Cannot re-assign already existing DataBlock for ModBus DataArea")
				i = i + 1

		self.next_stage = 1
		self.initialised = False


	def close(self):
		if self.is_server == True and self.server_process != None:
			print("Terminating server ...")
			put(self.thread_cmd_queue,3,'QUIT')

		if self.is_server == False and self.client_process != None:
			print("Terminating client ...")
			put(self.thread_cmd_queue,3,'QUIT')



	def set_out_parameters(self,ERROR,STATUS_MODBUS,STATUS_CONN,STATUS_FUNC,IDENT_CODE,CONN_ESTABLISHED,BUSY):
		self.ERROR = ERROR
		self.STATUS_MODBUS = STATUS_MODBUS
		self.STATUS_CONN = STATUS_CONN
		self.STATUS_FUNC = STATUS_FUNC
		self.IDENT_CODE = IDENT_CODE
		self.CONN_ESTABLISHED = CONN_ESTABLISHED
		self.BUSY = BUSY


	def LOG(self,direction,msg,log_time):
		log_file = conf_directory + "/logs/node_" + str(self.cpu.local_id) + "_log"
		with open(log_file,"a") as f:
			f.write(str(log_time) + "," + direction + "," + msg + "\n")
		return

		
	def hostname_to_ip(self,hostname) :

		host_id = int(hostname)
		conf_file = conf_directory + "/PLC_Config/lxc" + str(host_id) + "-0"

		if os.path.isfile(conf_file):

			lines = [line.rstrip('\n') for line in open(conf_file)]			
			for line in lines :
				line = ' '.join(line.split())
				line = line.split('=')
				if len(line) > 1 :
					#print(line)
					parameter = line[0]
					value= line[1]
					if "lxc.network.ipv4" in parameter :
						value = value.replace(' ',"")
						value = value.split("/")
						print("Dest IP : ", value[0])
						resolved_hostname = value[0]
						break
		else:
			resolved_hostname = "127.0.0.1"


		print("Resolved hostname = ",resolved_hostname)
		i = 0
		while(1) :
			if os.path.isfile(conf_directory + "/PLC_Config/lxc" + str(i) + "-0") :
				conf_file = conf_directory + "/PLC_Config/lxc" + str(i) + "-0"
				lines = [line.rstrip('\n') for line in open(conf_file)]			
				for line in lines :
					line = ' '.join(line.split())
					line = line.split('=')
					if len(line) > 1 :
						#print(line)
						parameter = line[0]
						value= line[1]
						if "lxc.network.ipv4" in parameter :
							value = value.replace(' ',"")
							value = value.split("/")
							resolved_IDS_IP = value[0]
							break				
			else :
				break
			i = i + 1

		self.IDS_IP = resolved_IDS_IP
		print("IP of IDS : ",self.IDS_IP)

		return resolved_hostname


	def frame_outgoing_message(self,msg) :

		new_msg = bytearray()
		new_msg.append(START_END_FLAG)
		i = 0

		while i < len(msg) :
			if msg[i] == START_END_FLAG :
				new_msg.append(ESCAPE_FLAG)
				new_msg.append(msg[i]^ 0x20)
			elif msg[i] == ESCAPE_FLAG :
				new_msg.append(ESCAPE_FLAG)
				new_msg.append(msg[i]^ 0x20)
			else :
				new_msg.append(msg[i])
			i = i + 1
		new_msg.append(START_END_FLAG)
		return new_msg

	def process_incoming_frame(self,msg) :

		new_msg = bytearray()
		i = 0
		while i < len(msg) :
			if msg[i] == START_END_FLAG :
				i = i + 1
			elif msg[i] == ESCAPE_FLAG :
				new_msg.append(msg[i+1]^0x20)
				i = i + 2
			else :
				new_msg.append(msg[i])
				i = i + 1

		return new_msg



	def set_connection_param_fields(self,db) :
		self.connection_params.set_connection_param_fields(self.cpu,db)	
		self.connection_params.store_connection_params(self.cpu,db)
		self.connection_params.load_connection_params(db)
		

	def set_timeouts(self,recv_time,conn_time) :
		self.recv_time = recv_time
		self.conn_time = conn_time
		


	

	def set_all_in_params_client(self,enq_enr,disconnect,recv_time,conn_time,unit,data_type,write_read,TI,length,start_address) :
		self.param_init_lock.acquire()

		self.set_timeouts(recv_time,conn_time)
		self.disconnect = disconnect
		self.unit = unit
		self.TI = TI
		self.data_type = data_type
		self.write_read = write_read
		self.length = length
		self.start_address = start_address
	
		self.param_init_lock.release()

		self.ENQ_ENR = enq_enr
		

	def set_all_in_params_server(self,enq_enr,disconnect,recv_time,conn_time) :

		self.param_init_lock.acquire()
		self.ENQ_ENR  =  enq_enr
		self.set_timeouts(recv_time,conn_time)
		self.disconnect = disconnect
		self.param_init_lock.release()

	def get_all_out_params_client(self) :
		self.param_init_lock.acquire()
		temp  =  (self.ERROR,self.BUSY,self.CONN_ESTABLISHED,self.STATUS_MODBUS,self.STATUS_CONN,self.STATUS_FUNC,self.IDENT_CODE)
		self.param_init_lock.release()
		return temp
	
	def get_all_out_params_server(self) :
		self.param_init_lock.acquire()
		temp =  (self.unit,self.TI,self.data_type,self.write_read,self.length,self.start_address,self.ERROR,self.BUSY,self.CONN_ESTABLISHED,self.STATUS_MODBUS,self.STATUS_CONN,self.STATUS_FUNC,self.IDENT_CODE)
		self.param_init_lock.release()
		return temp

	def send_dummy_ack_command(self) :
		put(self.thread_cmd_queue,1,[1])

	def get_updated_status(self,isServer):
		o = None
		o = get(self.thread_resp_queue,block=False)
		if o != None and isServer == True:
			if len(o) >= 6 :
				self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
				self.send_dummy_ack_command()

		if o != None and isServer == False:
			if len(o) >= 6 :
				self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
				if self.read_finish_status != 0 :
					msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
					put(self.thread_cmd_queue,2,(self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))					
					



	# input params should be set before call
	# all output params will be set by the call
	def call_modbus_server_process(self) :
		timeout = 0.001
		
		if self.STATUS == NOT_STARTED  : # start server

			if self.ENQ_ENR == 1 :
				self.STATUS = RUNNING	
				self.ERROR = False
				self.STATUS_MODBUS = 0x0000
				self.STATUS_CONN = 0x0000
				self.STATUS_FUNC = "MODBUSPN"
				self.IDENT_CODE = "NONE"
			

				if self.cpu.network_interface_type == 0 : #IP							
					self.server_process = Process(target = test_run_server_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id,self.disconnect,self.recv_time,self.conn_time,self.IDS_IP,self.cpu.local_id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
					self.server_process.start()
				else :
					
					self.server_process = Process(target = test_run_server_serial, args =(self.thread_resp_queue,self.thread_cmd_queue,self.disconnect,self.recv_time,self.conn_time,self.local_host_id,self.remote_host_id,self.connection_params.id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
					self.server_process.start()

				self.get_updated_status(True)
				
										
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 			# completed 
			if self.read_finish_status == 0 :	# for this call return the status params undisturbed
												# start new thread or resume the existing thread in the nxt call
				self.read_finish_status = 1
			

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True : # if disconnect is false
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING
					self.get_updated_status(True)
								
							
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
					# all these cases imply thread exited - if pos in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1  :
					isrestart = False
					if self.cpu.network_interface_type == 0 or (self.cpu.network_interface_type != 0 and self.STATUS != DONE)  :
						isrestart = True

					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"

					if self.server_process != None and isrestart == True:
						self.server_process.join()

						del self.thread_resp_queue
						del self.thread_cmd_queue

						self.thread_resp_queue = Array('i',range(2000))
						self.thread_cmd_queue = Array('i', range(2000))
						self.thread_resp_arr = Array('i', range(2000))
						self.thread_cmd_arr = Array('i', range(2000))
						
					if self.cpu.network_interface_type == 0 : #IP	
						self.server_process = Process(target = test_run_server_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id,self.disconnect,self.recv_time,self.conn_time,self.IDS_IP,self.cpu.local_id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
						print("Re-Started server process at " + str(time.time()))					
						self.server_process.start()
					elif isrestart == True :
						self.server_process = Process(target = test_run_server_serial, args =(self.thread_resp_queue,self.thread_cmd_queue,self.disconnect,self.recv_time,self.conn_time,self.local_host_id,self.remote_host_id,self.connection_params.id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  						
						print("Re-Started server process at " + str(time.time()))					
						self.server_process.start()


					self.get_updated_status(True)
					
					o = None
					o = get(self.thread_resp_arr,block=False)
					if o != None :
						recv_data = o
						response,request_msg_params = self.mod_server.process_request_message(recv_data)										
						if request_msg_params["ERROR"] == NO_ERROR :
							self.STATUS = DONE
							self.ERROR = False						
							put(self.thread_cmd_arr,1,response)
							if self.disconnect == True :
								self.CONN_ESTABLISHED = False
								self.BUSY = False
								self.read_finish_status = 1
							else :
								if self.cpu.network_interface_type == 0 :
									self.CONN_ESTABLISHED = True
								else :
									self.CONN_ESTABLISHED = False
								self.BUSY = True
								self.read_finish_status = 0
						else :
							# set STATUS_MODBUS Based on returned error value			
							print("ERROR on processing request msg")
							self.read_finish_status = 1
							self.STATUS = DONE
							self.ERROR = True
							self.CONN_ESTABLISHED = False
							self.BUSY = False
							self.STATUS_MODBUS = request_msg_params["ERROR"]
							self.STATUS_CONN = 0x0
							put(self.thread_cmd_arr,3,'QUIT')



						self.unit = request_msg_params["unit"]
						self.TI = request_msg_params["ti"]
						self.data_type = request_msg_params["data_type"]
						self.write_read = request_msg_params["write_read"]
						self.start_address = request_msg_params["start_address"]
						self.length = request_msg_params["length"]
					
					
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.get_updated_status(True)
					
			o = None
			o = get(self.thread_resp_arr,block=False)
			if o != None :
				recv_data = o
				response,request_msg_params = self.mod_server.process_request_message(recv_data)										
				if request_msg_params["ERROR"] == NO_ERROR :
					self.STATUS = DONE
					self.ERROR = False						
					put(self.thread_cmd_arr,1,response)
					if self.disconnect == True :
						self.CONN_ESTABLISHED = False
						self.BUSY = False
						self.read_finish_status = 1
					else :
						if self.cpu.network_interface_type == 0 :
							self.CONN_ESTABLISHED = True
						else :
							self.CONN_ESTABLISHED = False

						self.BUSY = True
						self.read_finish_status = 0
				else :
					# set STATUS_MODBUS Based on returned error value			
					print("ERROR on processing request msg")
					self.read_finish_status = 1
					self.STATUS = DONE
					self.ERROR = True
					self.CONN_ESTABLISHED = False
					self.BUSY = False
					self.STATUS_MODBUS = request_msg_params["ERROR"]
					self.STATUS_CONN = 0x0
					put(self.thread_cmd_arr,3,'QUIT')



				self.unit = request_msg_params["unit"]
				self.TI = request_msg_params["ti"]
				self.data_type = request_msg_params["data_type"]
				self.write_read = request_msg_params["write_read"]
				self.start_address = request_msg_params["start_address"]
				self.length = request_msg_params["length"]

		return self.STATUS


	def call_modbus_client_process(self) :
		TCP_REMOTE_IP  = "127.0.0.1"
		TCP_REMOTE_IP  = self.connection_params.rem_staddr
		TCP_REMOTE_PORT = self.connection_params.rem_tsap_id
	
		timeout = 0.001


		
		if self.STATUS == NOT_STARTED  : # start server

			if self.ENQ_ENR == 1 :
				self.STATUS = RUNNING	
				self.ERROR = False
				self.STATUS_MODBUS = 0x0000
				self.STATUS_CONN = 0x0000
				self.STATUS_FUNC = "MODBUSPN"
				self.IDENT_CODE = "NONE"
				

				if self.cpu.network_interface_type == 0 : #IP					
					self.client_process = Process(target = test_run_client_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id, self.IDS_IP, TCP_REMOTE_IP, TCP_REMOTE_PORT,self.conn_time,self.cpu.local_id, self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
					self.client_process.start()
				else :
					#thread_resp_queue,thread_cmd_queue,conn_time, local_id, remote_id, connection_id, thread_resp_arr,thread_cmd_arr
					self.client_process = Process(target = test_run_client_serial, args =(self.thread_resp_queue,self.thread_cmd_queue,self.conn_time,self.local_host_id,self.remote_host_id,self.connection_params.id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
					self.client_process.start()

				self.get_updated_status(False)
					
					
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 	# completed 
			if self.read_finish_status == 0 :	# for this call return the status params undisturbed
												# start new thread or resume the existing thread in the nxt call
				if self.ENQ_ENR == 1 and self.CONN_ESTABLISHED == True:
					msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
					put(self.thread_cmd_arr,2,(self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = 0x0
					self.BUSY = True
					self.CONN_ESTABLISHED = True
				elif self.ENQ_ENR == 1:
					self.send_dummy_ack_command()
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = 0x0
					self.BUSY = True
					self.CONN_ESTABLISHED = False

				else:
					put(self.thread_cmd_queue,3,'QUIT')
					

				self.read_finish_status = 1
				

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True : # if disconnect is false
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING
					self.get_updated_status(False)
					
						
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
					# all these cases imply thread exited - if pos in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1  :
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"	
					
					
					if self.client_process != None:
						self.client_process.join()

					del self.thread_resp_queue
					del self.thread_cmd_queue

					self.thread_resp_queue = Array('i', range(2000))
					self.thread_cmd_queue = Array('i', range(2000))						
					self.thread_resp_arr = Array('i', range(2000))
					self.thread_cmd_arr = Array('i', range(2000))
						
					if self.cpu.network_interface_type == 0 : #IP	
						self.client_process = Process(target = test_run_client_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id, self.IDS_IP,TCP_REMOTE_IP, TCP_REMOTE_PORT,self.conn_time,self.cpu.local_id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
						self.client_process.start()
					else :
						self.client_process = Process(target = test_run_client_serial, args =(self.thread_resp_queue,self.thread_cmd_queue,self.conn_time,self.local_host_id,self.remote_host_id,self.connection_params.id,self.thread_resp_arr,self.thread_cmd_arr,self.conf_dir))  
						self.client_process.start()
					
					self.get_updated_status(False)
						
					o = None
					o = get(self.thread_resp_arr,block=False)
					if o != None :
						ERROR_CODE = self.mod_client.process_response_message(o)
						self.read_finish_status = 0
						if ERROR_CODE == NO_ERROR :
							self.STATUS = DONE
							self.ERROR = False
							if self.disconnect == True :
								self.read_finish_status = 1
								self.STATUS = DONE
								self.CONN_ESTABLISHED = False
								put(self.thread_cmd_arr,3,'QUIT')					
						else :
							# set STATUS_MODBUS Based on returned error value
							self.read_finish_status = 1			
							self.STATUS = DONE
							self.ERROR = True
							self.STATUS_MODBUS = ERROR_CODE
							self.STATUS_CONN = 0x0
							self.CONN_ESTABLISHED = False
							put(self.thread_cmd_arr,3,'QUIT')
					
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.get_updated_status(False)
				
			o = None
			o = get(self.thread_resp_arr,block=False)
			if o != None :
				ERROR_CODE = self.mod_client.process_response_message(o)
				self.read_finish_status = 0
				if ERROR_CODE == NO_ERROR :
					self.STATUS = DONE
					self.ERROR = False
					if self.disconnect == True :
						self.read_finish_status = 1
						self.STATUS = DONE
						self.CONN_ESTABLISHED = False
						put(self.thread_cmd_arr,3,'QUIT')					
				else :
					# set STATUS_MODBUS Based on returned error value
					self.read_finish_status = 1			
					self.STATUS = DONE
					self.ERROR = True
					self.STATUS_MODBUS = ERROR_CODE
					self.STATUS_CONN = 0x0
					self.CONN_ESTABLISHED = False
					put(self.thread_cmd_arr,3,'QUIT')						
				
									

		return self.STATUS

	def get_status(self) :
		return self.STATUS


	
