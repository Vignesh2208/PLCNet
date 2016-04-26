
from awlsim.core.systemblocks.connection_params import *
import math
import socket
import select
import sys
#import queue
#from queue import *
import multiprocessing, Queue, Array
#from multiprocessing import Queue as Queue
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
				self.thread_cmd_queue = multiprocessing.Queue()
				no_error = True
			except :
				no_error = False

				
		no_error = False
		while no_error == False :
			try:
				self.thread_resp_queue = multiprocessing.Queue()
				no_error = True
			except :
				no_error = False
				

		no_error = False
		while no_error == False :
			try:
				self.thread_param_queue = multiprocessing.Queue()
				no_error = True
			except :
				no_error = False


		self.thread_resp_arr = Array('i', range(2000))
		self.thread_cmd_arr = Array('i', range(2000))
				

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
			self.thread_cmd_queue.put('QUIT')

		if self.is_server == False and self.client_process != None:
			print("Terminating client ...")
			self.thread_cmd_queue.put('QUIT')



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
		#self.connection_params.print_connection_params()

	def set_timeouts(self,recv_time,conn_time) :
		self.recv_time = recv_time
		self.conn_time = conn_time
		

	def run_server_ip(self) :

		
		self.read_finish_status = 1
		TCP_LOCAL_PORT = self.connection_params.local_tsap_id
		BUFFER_SIZE = 4096
		self.BUSY = True
		server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		#server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		#server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		try:
			ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		except socket.error as sockerror:
			print("ERRor creating socket")
			print(sockerror)
		ids_host = str(self.IDS_IP);
		ids_port = 8888; 

		server_socket.settimeout(self.conn_time)
		local_id = str(self.cpu.local_id)
		server_host_ip = self.hostname_to_ip(local_id)
		server_socket.bind(('0.0.0.0',TCP_LOCAL_PORT))	# bind to any address
		server_socket.listen(1)

		print("Start time = ", time.time())
		print("Listening on port " + str(TCP_LOCAL_PORT))	
				
		
		try:
			client_socket, address = server_socket.accept()
			#client_socket, address = server_socket.recvfrom(2048)
		except socket.timeout:
			print("End time = ", time.time())
			print("Socket TIMEOUT Done !!!!!!!!!!!!!!!!")
			self.status_lock.acquire()
			self.read_finish_status = 0
			self.STATUS = CONN_TIMEOUT_ERROR
			self.ERROR = True
			self.STATUS_MODBUS = 0x0
			self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
			self.BUSY = False
			self.CONN_ESTABLISHED = False
			self.thread_resp_queue.put(1)
			self.status_lock.release()
			return			
		


		#client_socket, address = server_socket.accept()
		print("Established Connection from " + str(address))
		self.CONN_ESTABLISHED = True
		#client_socket.settimeout(self.recv_time)
		while True:
			
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()	
					
			try:
				data = client_socket.recv(BUFFER_SIZE)
			except socket.timeout:

				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = SERVER_ERROR
				self.ERROR = True
				self.STATUS_MODBUS = ERROR_UNKNOWN_EXCEPTION
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break

			recv_time = time.time()
			#recv_time = datetime.now()
			recv_data = bytearray(data)
			print("Recv data = ",recv_data)	
		
        	#Set the whole string
			msg = str(self.cpu.local_id) + "," + str(recv_time) + ",RECV," + str(recv_data)
			try :			
				ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
				pass
			except socket.error as sockerror :
				print(sockerror)
			response,request_msg_params = self.mod_server.process_request_message(recv_data)
			

			if request_msg_params["ERROR"] == NO_ERROR :
				self.ERROR = False
			else :
				# set STATUS_MODBUS Based on returned error value
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True
				self.STATUS_MODBUS = request_msg_params["ERROR"]
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break


			# write all inout params
			self.param_init_lock.acquire()
			self.unit = request_msg_params["unit"]
			self.TI = request_msg_params["ti"]
			self.data_type = request_msg_params["data_type"]
			self.write_read = request_msg_params["write_read"]
			self.start_address = request_msg_params["start_address"]
			self.length = request_msg_params["length"]
			self.param_init_lock.release()

			#self.thread_resp_queue.put(1)
			#self.thread_cmd_queue.get()
			
			client_socket.send(response)
			print("Sent response to client = ",response)
			msg = str(self.cpu.local_id) + "," + str(time.time())  + ",SEND," + str(response)
			try :
			
				ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
				pass
			except socket.error as sockerror :
				print(sockerror)


			
			if self.disconnect == True :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.thread_resp_queue.put(1)
				client_socket.close()
				break
			else :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.status_lock.release()
				self.thread_resp_queue.put(1)
				self.thread_cmd_queue.get()
		self.BUSY = False
		self.CONN_ESTABLISHED = False
		self.status_lock.release()


	
	def run_client_ip(self) :
		self.read_finish_status = 1
		TCP_REMOTE_IP  = '127.0.0.1'
		#TCP_REMOTE_IP  = self.connection_params.rem_staddr

		TCP_REMOTE_PORT = self.connection_params.rem_tsap_id
		BUFFER_SIZE = 4096
		self.BUSY = True
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		except socket.error as sockerror:
			print("Error creating socket")
			print(sockerror)
		ids_host = str(self.IDS_IP);
		ids_port = 8888; 


		print("Start time = ", time.time())
		
		try:
			client_socket.settimeout(self.conn_time)
			client_socket.connect((TCP_REMOTE_IP,TCP_REMOTE_PORT))
		except socket.error as socketerror:
			print(TCP_REMOTE_IP,TCP_REMOTE_PORT)
			print(socketerror)
			self.status_lock.acquire()
			self.read_finish_status = 0
			self.STATUS = CONN_TIMEOUT_ERROR
			self.ERROR = True
			self.STATUS_MODBUS = 0x0
			self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
			self.BUSY = False
			self.CONN_ESTABLISHED = False
			self.status_lock.release()
			self.thread_resp_queue.put(1)
			return

		self.CONN_ESTABLISHED = True
		##self.thread_resp_queue.put(1)
		##self.thread_cmd_queue.get()
		
		while True :

			# read all input and inout params
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()

			print("Resumed connection")
			sys.stdout.flush()

			self.param_init_lock.acquire()
			recv_time = self.recv_time
			data_type = self.data_type
			write_read = self.write_read
			length = self.length
			single_write = self.connection_params.single_write
			start_address = self.start_address
			TI = self.TI
			self.param_init_lock.release()

			client_socket.settimeout(self.recv_time)

			msg_to_send,exception = self.mod_client.construct_request_message(data_type,write_read,length,single_write,start_address,TI,self.remote_host_id)

			if msg_to_send == None :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True 
				self.STATUS_MODBUS = exception
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break
			
			try:
				print("Sent msg = ",msg_to_send)
				client_socket.send(msg_to_send)
			except socket.error as socketerror :
				print("Client Error : ", socketerror)
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = CLIENT_ERROR
				self.ERROR = True
				self.STATUS_MODBUS = ERROR_UNKNOWN_EXCEPTION
				self.STATUS_CONN  = 0x0
				self.thread_resp_queue.put(1)
				break

			msg = str(self.cpu.local_id) + "," + str(time.time()) + ",SEND," + str(msg_to_send)
			try :			
				ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
				pass
			except socket.error as sockerror :
				print(sockerror)

			print("Waiting for cmd queue get")
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()
			print("Resumed from cmd queue get")
			
			try:
				data = client_socket.recv(BUFFER_SIZE)
			except socket.timeout:
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = RECV_TIMEOUT_ERROR
				self.ERROR = True
				self.STATUS_MODBUS = 0x0
				self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
				self.thread_resp_queue.put(1)
				break
			recv_time = time.time()
			#recv_time = datetime.now()
			recv_data = bytearray(data)
			print("Response from server : ",recv_data)
			

			msg = str(self.cpu.local_id) + "," + str(recv_time) + ",RECV," + str(recv_data)
			try :			
				ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
				pass
			except socket.error as sockerror :
				print(sockerror)


			ERROR_CODE = self.mod_client.process_response_message(recv_data)
			if ERROR_CODE == NO_ERROR :
				self.ERROR = False
			else :
				# set STATUS_MODBUS Based on returned error value
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True						
				self.STATUS_MODBUS = ERROR_CODE
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break

			if self.disconnect == True :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.thread_resp_queue.put(1)
				client_socket.close()
				break
			else :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.status_lock.release()
				self.thread_resp_queue.put(1)
				self.thread_cmd_queue.get()
				
				
	
		self.BUSY = False
		self.CONN_ESTABLISHED = False
		self.status_lock.release()

	def run_server_serial(self) :
		self.read_finish_status = 1
		self.BUSY = True
		
		server_fd = os.open("/dev/s3fserial" + str(self.connection_params.id) ,os.O_RDWR)

		READ_ONLY = select.POLLIN 
		WRITE_ONLY = select.POLLOUT		
		
		print("Start time = ", time.time())
		set_connection.set_connection(self.local_host_id,self.remote_host_id,self.connection_params.id)

		recv_msg = bytearray()

		while True :

			isfirst = 1			
			poller = select.poll()
			poller.register(server_fd, READ_ONLY)
			print ("poll start time :", time.time())
			events = poller.poll(self.recv_time*1000)
			temp_time = time.time()
			if isfirst == 1 :
				log_time = temp_time
				isfirst = 0
			if len(events) == 0 :
				print("End time = ", time.time())
				print("Serial Recv TIMEOUT Done !!!!!!!!!!!!!!!!")
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = RECV_TIMEOUT_ERROR
				self.ERROR = True
				self.STATUS_MODBUS = 0x0
				self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
				self.BUSY = False
				self.CONN_ESTABLISHED = False
				self.thread_resp_queue.put(1)
				self.status_lock.release()
				return

			self.status_lock.acquire()
			if self.CONN_ESTABLISHED == False :
				self.CONN_ESTABLISHED = True				
				self.status_lock.release()
				## changed this. put it as the last statement of while loop
				self.thread_resp_queue.put(1)
				self.thread_cmd_queue.get()
			else :
				self.status_lock.release()
				pass
				#if isfirst == 1 :
				#	log_time = time.time()
				#	isfirst = 0

			

			data = os.read(server_fd,100)
			recv_msg.extend(data)
			data = bytearray(data)
			if data[len(data) - 1] == START_END_FLAG :
					# got full data
				resume_poll = False
			else :
				resume_poll = True
			

			if resume_poll == True :
				continue
			
			print("Recv msg = ",recv_msg)
			self.LOG("RECV",str(recv_msg),log_time)
			recv_data = self.process_incoming_frame(recv_msg)
			print("Recv data = ",recv_data)
			
			response,request_msg_params = self.mod_server.process_request_message(recv_data)
			

			if request_msg_params["ERROR"] == NO_ERROR :
				self.ERROR = False
			else :
				# set STATUS_MODBUS Based on returned error value
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True
				self.STATUS_MODBUS = request_msg_params["ERROR"]
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break


			# write all inout params
			self.param_init_lock.acquire()
			self.unit = request_msg_params["unit"]
			self.TI = request_msg_params["ti"]
			self.data_type = request_msg_params["data_type"]
			self.write_read = request_msg_params["write_read"]
			self.start_address = request_msg_params["start_address"]
			self.length = request_msg_params["length"]
			self.param_init_lock.release()

			response = self.frame_outgoing_message(response)

			n_wrote = 0
			while n_wrote < len(response) :
				poller = select.poll()
				poller.register(server_fd, WRITE_ONLY)
				events = poller.poll(self.conn_time*1000)
				if len(events) == 0 :
					print("End time = ", time.time())
					print("Serial Send TIMEOUT Done !!!!!!!!!!!!!!!!")
					self.status_lock.acquire()
					self.read_finish_status = 0
					self.STATUS = CONN_TIMEOUT_ERROR
					self.ERROR = True
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
					self.BUSY = False
					self.CONN_ESTABLISHED = False
					self.thread_resp_queue.put(1)
					self.status_lock.release()
					return
				if n_wrote == 0 :
					log_time = time.time()
				n_wrote = n_wrote + os.write(server_fd,response[n_wrote:])			

			#log_time = time.time()
			self.LOG("SEND",str(response),log_time)
			print("Sent response to client = ",response)
			
			if self.disconnect == True :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.thread_resp_queue.put(1)
				break
			else :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				recv_msg = bytearray()
				self.status_lock.release()
				self.thread_resp_queue.put(1)
				self.thread_cmd_queue.get()

			# before resuming connection if disconnect is false. added 3 new steps
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()
			self.status_lock.acquire()
			self.CONN_ESTABLISHED = False 
			self.status_lock.release()

		self.BUSY = False
		self.CONN_ESTABLISHED = False
		self.status_lock.release()

			
	def run_client_serial(self) :
		self.read_finish_status = 1
		self.BUSY = True
		
		client_fd = os.open("/dev/s3fserial" + str(self.connection_params.id) ,os.O_RDWR)

		READ_ONLY = select.POLLIN 
		WRITE_ONLY = select.POLLOUT		
		
		print("Start time = ", time.time())
		set_connection.set_connection(self.local_host_id,self.remote_host_id,self.connection_params.id)

				
		while True :
			# read all input and inout params
			self.param_init_lock.acquire()
			recv_time = self.recv_time
			data_type = self.data_type
			write_read = self.write_read
			length = self.length
			single_write = self.connection_params.single_write
			start_address = self.start_address
			TI = self.TI
			self.param_init_lock.release()

			# New steps to handle disconnect = false
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()
			self.status_lock.acquire()
			self.CONN_ESTABLISHED = False
			self.status_lock.release()


			msg_to_send,exception = self.mod_client.construct_request_message(data_type,write_read,length,single_write,start_address,TI,self.remote_host_id)

			if msg_to_send == None :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True 
				self.STATUS_MODBUS = exception
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break
			
			
			print("data to send = ",msg_to_send)
			send_msg = self.frame_outgoing_message(msg_to_send)
			print("msg to send = ", send_msg)
			n_wrote = 0
			while n_wrote < len(send_msg) :
				poller = select.poll()
				poller.register(client_fd, WRITE_ONLY)
				events = poller.poll(self.conn_time*1000)
				log_time = None
				if len(events) == 0 :
					print("End time = ", time.time())
					print("Serial Write TIMEOUT Done !!!!!!!!!!!!!!!!")
					self.status_lock.acquire()
					self.read_finish_status = 0
					self.STATUS = CONN_TIMEOUT_ERROR
					self.ERROR = True
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
					self.BUSY = False
					self.CONN_ESTABLISHED = False
					self.thread_resp_queue.put(1)
					self.status_lock.release()
					return

				self.status_lock.acquire()
				if self.CONN_ESTABLISHED == False:
					self.CONN_ESTABLISHED = True
					self.status_lock.release()
					if n_wrote == 0 :
						log_time = time.time()
					self.thread_resp_queue.put(1)
					self.thread_cmd_queue.get()
				else :
					self.status_lock.release()
					pass
				n_wrote = n_wrote + os.write(client_fd,send_msg[n_wrote:])

			
			if log_time == None :
				log_time = time.time()		
			self.LOG("SEND",str(send_msg),log_time)	
			
			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()

			# receive response from server
			recv_msg = bytearray()
			isfirst = 1
			while True :
				poller = select.poll()
				poller.register(client_fd, READ_ONLY)
				#print ("poll start time :", time.time())
				events = poller.poll(self.recv_time*1000)
				temp_time = time.time()
				if isfirst == 1 :
					log_time = temp_time
					isfirst = 0
				
				if len(events) == 0 :
					print("End time = ", time.time())
					print("Serial Read TIMEOUT Done !!!!!!!!!!!!!!!!")
					self.status_lock.acquire()
					self.read_finish_status = 0
					self.STATUS = RECV_TIMEOUT_ERROR
					self.ERROR = True
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
					self.BUSY = False
					self.CONN_ESTABLISHED = False
					self.thread_resp_queue.put(1)
					self.status_lock.release()
					return


				#while 1 :
				data = os.read(client_fd,100)
				#	if data != None and len(data) != 0 :
				#		if is_first == 1 :
				#			log_time = time.time()
				#			isfirst = 0
				#		break


				recv_msg.extend(data)
				data = bytearray(data)
				if data[len(data) - 1] == START_END_FLAG :
					resume_poll = False
				else :
					resume_poll = True

				if resume_poll == True :
					continue
				else :
					break
			
			print("Recv msg = ",recv_msg)
			self.LOG("RECV",str(recv_msg),log_time)
			recv_data = self.process_incoming_frame(recv_msg)
			print("Response from server = ",recv_data)			
			ERROR_CODE = self.mod_client.process_response_message(recv_data)


			if ERROR_CODE == NO_ERROR :
				self.ERROR = False
			else :
				# set STATUS_MODBUS Based on returned error value
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = True						
				self.STATUS_MODBUS = ERROR_CODE
				self.STATUS_CONN = 0x0
				self.thread_resp_queue.put(1)
				break

			if self.disconnect == True :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.thread_resp_queue.put(1)
				
				break
			else :
				self.status_lock.acquire()
				self.read_finish_status = 0
				self.STATUS = DONE
				self.ERROR = False
				self.status_lock.release()
				self.thread_resp_queue.put(1)
				self.thread_cmd_queue.get()
				
	
		self.BUSY = False
		self.CONN_ESTABLISHED = False
		self.status_lock.release()



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


	# input params should be set before call
	# all output params will be set by the call
	def call_modbus_server(self) :
		self.status_lock.acquire()
		if self.STATUS == NOT_STARTED  : # start server

			if self.ENQ_ENR == 1 :
				self.STATUS = RUNNING	
				self.ERROR = False
				self.STATUS_MODBUS = 0x0000
				self.STATUS_CONN = 0x0000
				self.STATUS_FUNC = "MODBUSPN"
				self.IDENT_CODE = "NONE"
				self.status_lock.release()

				if self.cpu.network_interface_type == 0 : #IP							
					#threading.Thread(target=self.run_server_ip).start()
					producer = Process(target = test_run_server_ip, args =(self.status_lock,self.param_init_lock,self.thread_resp_queue,self.thread_cmd_queue,self))  
					producer.start()
				else :
					
					threading.Thread(target=self.run_server_serial).start()

				try :
					o = self.thread_resp_queue.get(block=True,timeout=0.1)					
				except:
					o = None
					pass
				if o != None :
					self.thread_cmd_queue.put(1)
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 	# completed 
			if self.read_finish_status == 0 :	# for this call return the status params undisturbed
												# start new thread or resume the existing thread in the nxt call
				self.read_finish_status = 1
				self.status_lock.release()

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True : # if disconnect is false
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING
					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)					
					except:
						o = None
						pass
					if o != None :
						self.thread_cmd_queue.put(1) # resume connection
					
				self.status_lock.release()
				
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
				# all these cases imply thread exited - if pos in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1  :
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"	
					self.status_lock.release()
					if self.cpu.network_interface_type == 0 : #IP							
						threading.Thread(target=self.run_server_ip).start()
					else:
						
						threading.Thread(target=self.run_server_serial).start()

					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)
					except:
						o = None
						pass
					if o != None :
						self.thread_cmd_queue.put(1)
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.status_lock.release()
			try :
				o = self.thread_resp_queue.get(block=True,timeout=0.1)				
			except:
				o = None
				pass

			if o != None :
				self.thread_cmd_queue.put(1)

		return self.STATUS

	# input params should be set before call
	# all output params will be set by the call
	def call_modbus_client(self) :

		self.status_lock.acquire()
		if self.STATUS == NOT_STARTED : # start server
			if self.ENQ_ENR == 1 and self.PREV_ENQ_ENR == 0 :
				self.STATUS = RUNNING
				self.ERROR = False
				self.STATUS_MODBUS = 0x0000
				self.STATUS_CONN = 0x0000
				self.STATUS_FUNC = "MODBUSPN"
				self.IDENT_CODE = "NONE"	
				self.status_lock.release()
				
				#nsleep(5000000)			
				
				if self.cpu.network_interface_type == 0 : #IP
					#time.sleep(1)
					#nsleep(3000000)
					time.sleep(2)
					threading.Thread(target=self.run_client_ip).start()
				else:
					
					threading.Thread(target=self.run_client_serial).start()

				try :
					o = self.thread_resp_queue.get(block=True,timeout=0.1)					
				except:
					o = None
					pass
				if o != None :
					self.thread_cmd_queue.put(1)
			self.PREV_ENQ_ENR = self.ENQ_ENR			

		elif self.STATUS != RUNNING  : 	# completed
			if self.read_finish_status == 0 :
				self.read_finish_status = 1
				self.status_lock.release()
			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True :
				#if self.ENQ_ENR == 1 and self.PREV_ENQ_ENR == 0 :
				if self.ENQ_ENR == 1:
					self.STATUS = RUNNING
					print("Resuming connection")
					sys.stdout.flush()
					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)						
					except:
						o = None
						pass
					if o != None :
						self.thread_cmd_queue.put(1)
					#self.thread_cmd_queue.put(1) # resume connection
				self.status_lock.release()
				
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
				# all these cases imply thread exited - if pos edge in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1 and self.PREV_ENQ_ENR == 0 :
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"	
					self.status_lock.release()
					if self.cpu.network_interface_type == 0 :
						threading.Thread(target=self.run_client_ip).start()
					else:
						
						threading.Thread(target=self.run_client_serial).start()
						
					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)						
					except:
						o = None
						pass
					if o != None :
						self.thread_cmd_queue.put(1)
					
				
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.status_lock.release()
			try :
				o = self.thread_resp_queue.get(block=True,timeout=0.1)				
			except:
				o = None
				pass

			if o != None :
				self.thread_cmd_queue.put(1)

		return self.STATUS



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
					self.server_process = Process(target = test_run_server_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id,self.disconnect,self.recv_time,self.conn_time,self.IDS_IP,self.cpu.local_id))  
					print("Started server process at : " + str(time.time()))					
					self.server_process.start()
				o = None
				try :
					#o = self.thread_resp_queue.get(block=True,timeout=timeout)					
					o = self.thread_resp_queue.get(block=False)					
				except Queue.Empty:
					o = None
					
				if o != None :
					if len(o) >= 6 :
						self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
						self.thread_cmd_queue.put(1)
					else:
						recv_data = o[0]
						#?self.thread_cmd_queue.put(1)
						#?response,request_msg_params = self.mod_server.process_request_message(recv_data)
						#?self.thread_cmd_queue.put((response,request_msg_params))
						response,request_msg_params = self.mod_server.process_request_message(recv_data)										
						if request_msg_params["ERROR"] == NO_ERROR :
							self.STATUS = DONE
							self.ERROR = False						
							self.thread_cmd_queue.put((response,request_msg_params))
							if self.disconnect == True :
								self.CONN_ESTABLISHED = False
								self.BUSY = False
								self.read_finish_status = 1
							else :
								self.CONN_ESTABLISHED = True
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
							self.thread_cmd_queue.put('QUIT')

						self.unit = request_msg_params["unit"]
						self.TI = request_msg_params["ti"]
						self.data_type = request_msg_params["data_type"]
						self.write_read = request_msg_params["write_read"]
						self.start_address = request_msg_params["start_address"]
						self.length = request_msg_params["length"]
						
						

					
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 	# completed 
			if self.read_finish_status == 0 :	# for this call return the status params undisturbed
												# start new thread or resume the existing thread in the nxt call
				self.read_finish_status = 1
			

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True : # if disconnect is false
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING
					o = None
					try :
						#o = self.thread_resp_queue.get(block=True,timeout=timeout)					
						o = self.thread_resp_queue.get(block=False)					
					except Queue.Empty:
						o = None
						
					if o != None :
						if len(o) >= 6 :
							self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
							self.thread_cmd_queue.put(1)
						else:
							#?recv_data = o[0]
							#?self.thread_cmd_queue.put(1)
							#?response,request_msg_params = self.mod_server.process_request_message(recv_data)
							#?self.thread_cmd_queue.put((response,request_msg_params))
							recv_data = o[0]					
							#?self.thread_cmd_queue.put(1)
							response,request_msg_params = self.mod_server.process_request_message(recv_data)										
							if request_msg_params["ERROR"] == NO_ERROR :
								self.STATUS = DONE
								self.ERROR = False						
								self.thread_cmd_queue.put((response,request_msg_params))
								if self.disconnect == True :
									self.CONN_ESTABLISHED = False
									self.BUSY = False
									self.read_finish_status = 1
								else :
									self.CONN_ESTABLISHED = True
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
								self.thread_cmd_queue.put('QUIT')

							self.unit = request_msg_params["unit"]
							self.TI = request_msg_params["ti"]
							self.data_type = request_msg_params["data_type"]
							self.write_read = request_msg_params["write_read"]
							self.start_address = request_msg_params["start_address"]
							self.length = request_msg_params["length"]
							
					
			
				
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
				# all these cases imply thread exited - if pos in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1  :
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"	
			
					if self.cpu.network_interface_type == 0 : #IP	
						if self.server_process != None:
							self.server_process.join()

						#?del self.thread_resp_queue
						#?del self.thread_cmd_queue

						#?self.thread_resp_queue = multiprocessing.Queue()
						#?self.thread_cmd_queue = multiprocessing.Queue()
						self.thread_cmd_queue = SharedQueue()
						self.thread_resp_queue = SharedQueue()

						self.thread_resp_arr = Array('i', range(2000))
						self.thread_cmd_arr = Array('i', range(2000))
						

						self.server_process = Process(target = test_run_server_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id,self.disconnect,self.recv_time,self.conn_time,self.IDS_IP,self.cpu.local_id))  
						print("Re-Started server process at " + str(time.time()))					
						self.server_process.start()

					o = None
					try :
						#o = self.thread_resp_queue.get(block=True,timeout=timeout)
						o = self.thread_resp_queue.get(block=False)					
					except Queue.Empty:
						o = None

					if o != None :
						if len(o) >= 6 :
							self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
							self.thread_cmd_queue.put(1)
						else:
							recv_data = o[0]
							
							#?self.thread_cmd_queue.put(1)
							response,request_msg_params = self.mod_server.process_request_message(recv_data)										
							if request_msg_params["ERROR"] == NO_ERROR :
								self.STATUS = DONE
								self.ERROR = False						
								self.thread_cmd_queue.put((response,request_msg_params))
								if self.disconnect == True :
									self.CONN_ESTABLISHED = False
									self.BUSY = False
									self.read_finish_status = 1
								else :
									self.CONN_ESTABLISHED = True
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
								self.thread_cmd_queue.put('QUIT')


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
			
			o = None
			try :
				#o = self.thread_resp_queue.get(block=True,timeout=timeout)				
				o = self.thread_resp_queue.get(block=False)					
			except Queue.Empty:
				o = None
				
			if o != None :
				if len(o) >= 6 :
					self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
					self.thread_cmd_queue.put(1)
				else:
					recv_data = o[0]					
					#?self.thread_cmd_queue.put(1)
					print("Server processing request message at " + str(datetime.datetime.now()))
					response,request_msg_params = self.mod_server.process_request_message(recv_data)										
					print("Server processed request message at " + str(datetime.datetime.now()))
					if request_msg_params["ERROR"] == NO_ERROR :
						self.STATUS = DONE
						self.ERROR = False						
						self.thread_cmd_queue.put((response,request_msg_params))
						if self.disconnect == True :
							self.CONN_ESTABLISHED = False
							self.BUSY = False
							self.read_finish_status = 1
						else :
							self.CONN_ESTABLISHED = True
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
						self.thread_cmd_queue.put('QUIT')



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

					
					self.client_process = Process(target = test_run_client_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id, self.IDS_IP, TCP_REMOTE_IP, TCP_REMOTE_PORT,self.conn_time,self.cpu.local_id, self.thread_resp_arr,self.thread_cmd_arr))  
					self.client_process.start()

				o = None
				try :
					#o = self.thread_resp_queue.get(block=True,timeout=timeout)					
					o = self.thread_resp_queue.get(block=False)					
				except Queue.Empty:
					o = None
					
				if o != None :
					if len(o) >= 6 :
						self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
						#msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
						#self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
						if self.read_finish_status != 0 :
							msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
							self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
					else:
						recv_data = o[0]
						ERROR_CODE = self.mod_client.process_response_message(recv_data)
						#?self.thread_cmd_queue.put(ERROR_CODE)						
						self.read_finish_status = 0
						if ERROR_CODE == NO_ERROR :
							self.STATUS = DONE
							self.ERROR = False
							if self.disconnect == True :
								self.read_finish_status = 1
								self.STATUS = DONE
								self.CONN_ESTABLISHED = False
								self.thread_cmd_queue('QUIT')					
						else :
							# set STATUS_MODBUS Based on returned error value			
							self.read_finish_status = 1
							self.STATUS = DONE
							self.ERROR = True
							self.STATUS_MODBUS = ERROR_CODE
							self.STATUS_CONN = 0x0
							self.CONN_ESTABLISHED = False
							self.thread_cmd_queue('QUIT')								

					
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 	# completed 
			if self.read_finish_status == 0 :	# for this call return the status params undisturbed
												# start new thread or resume the existing thread in the nxt call
				if self.ENQ_ENR == 1 and self.CONN_ESTABLISHED == True:
					msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
					self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
					
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = 0x0
					self.BUSY = True
					self.CONN_ESTABLISHED = True
				elif self.ENQ_ENR == 1:
					self.thread_cmd_queue.put(1)
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0
					self.STATUS_CONN = 0x0
					self.BUSY = True
					self.CONN_ESTABLISHED = False

				else:
					self.thread_cmd_queue.put('QUIT')

				self.read_finish_status = 1
				

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True : # if disconnect is false
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING

					o = None
					try :
						#o = self.thread_resp_queue.get(block=True,timeout=timeout)					
						o = self.thread_resp_queue.get(block=False)					
					except Queue.Empty:
						o = None
						
					if o != None :
						if len(o) >= 6 :
							self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
							#msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
							#self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
							if self.read_finish_status != 0 :
								msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
								self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
						else:
							recv_data = o[0]
							ERROR_CODE = self.mod_client.process_response_message(recv_data)
							#?self.thread_cmd_queue.put(ERROR_CODE)
							self.read_finish_status = 0
							if ERROR_CODE == NO_ERROR :
								self.STATUS = DONE
								self.ERROR = False
								if self.disconnect == True :
									self.read_finish_status = 1
									self.STATUS = DONE
									self.CONN_ESTABLISHED = False
									self.thread_cmd_queue('QUIT')					
							else :
								# set STATUS_MODBUS Based on returned error value
								self.read_finish_status = 1
								self.STATUS = DONE
								self.ERROR = True
								self.STATUS_MODBUS = ERROR_CODE
								self.STATUS_CONN = 0x0
								self.CONN_ESTABLISHED = False
								self.thread_cmd_queue('QUIT')						
				
			else :	# cases STATUS = DONE, CONN = False, STATUS = RECV_TIMEOUT_ERROR |  CONN_TIMEOUT_ERROR | CLIENT_ERROR, CONN = False
				# all these cases imply thread exited - if pos in ENQ_ENR - restart new thread
				if self.ENQ_ENR == 1  :
					self.STATUS = RUNNING
					self.ERROR = False
					self.STATUS_MODBUS = 0x0000
					self.STATUS_CONN = 0x0000
					self.STATUS_FUNC = "MODBUSPN"
					self.IDENT_CODE = "NONE"	
					
					if self.cpu.network_interface_type == 0 : #IP	
						if self.client_process != None:
							self.client_process.join()

						#?del self.thread_resp_queue
						#?del self.thread_cmd_queue

						#?self.thread_resp_queue = multiprocessing.Queue()
						#?self.thread_cmd_queue = multiprocessing.Queue()
						self.thread_cmd_queue = SharedQueue()
						self.thread_resp_queue = SharedQueue()
						self.thread_resp_arr = Array('i', range(2000))
						self.thread_cmd_arr = Array('i', range(2000))
						

						self.client_process = Process(target = test_run_client_ip, args =(self.thread_resp_queue,self.thread_cmd_queue,self.connection_params.local_tsap_id, self.IDS_IP,TCP_REMOTE_IP, TCP_REMOTE_PORT,self.conn_time,self.cpu.local_id))  
						self.client_process.start()
					
					o = None
					try :
						#o = self.thread_resp_queue.get(block=True,timeout=timeout)
						o = self.thread_resp_queue.get(block=False)
					except Queue.Empty:
						o = None

					if o != None :
						if len(o) >= 6 :
							self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
							#msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
							#self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
							if self.read_finish_status != 0 :
								msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
								self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
						else:
							recv_data = o[0]
							ERROR_CODE = self.mod_client.process_response_message(recv_data)
							self.read_finish_status = 0
							if ERROR_CODE == NO_ERROR :
								self.STATUS = DONE
								self.ERROR = False
								if self.disconnect == True :
									self.read_finish_status = 1
									self.STATUS = DONE
									self.CONN_ESTABLISHED = False
									self.thread_cmd_queue('QUIT')					
							else :
								# set STATUS_MODBUS Based on returned error value
								self.read_finish_status = 1			
								self.STATUS = DONE
								self.ERROR = True
								self.STATUS_MODBUS = ERROR_CODE
								self.STATUS_CONN = 0x0
								self.CONN_ESTABLISHED = False
								self.thread_cmd_queue('QUIT')					
							#?self.thread_cmd_queue.put(ERROR_CODE)
					
					
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			
			o = None
			try :
				#o = self.thread_resp_queue.get(block=True,timeout=timeout)
				o = self.thread_resp_queue.get(block=False)				
			except Queue.Empty:
				o = None
				
			if o != None :
				if len(o) >= 6 :
					self.read_finish_status,self.STATUS,self.ERROR,self.STATUS_MODBUS,self.STATUS_CONN,self.BUSY,self.CONN_ESTABLISHED = o
					if self.read_finish_status != 0 :
						msg_to_send,exception = self.mod_client.construct_request_message(self.data_type,self.write_read,self.length,self.connection_params.single_write,self.start_address,self.TI,self.remote_host_id)
						self.thread_cmd_queue.put((self.disconnect,self.recv_time, self.conn_time, msg_to_send, exception))
				else:
					recv_data = o[0]
					ERROR_CODE = self.mod_client.process_response_message(recv_data)
					self.read_finish_status = 0
					if ERROR_CODE == NO_ERROR :
						self.STATUS = DONE
						self.ERROR = False
						if self.disconnect == True :
							self.read_finish_status = 1
							self.STATUS = DONE
							self.CONN_ESTABLISHED = False
							self.thread_cmd_queue('QUIT')					
					else :
						# set STATUS_MODBUS Based on returned error value
						self.read_finish_status = 1
						self.STATUS = DONE
						self.ERROR = True
						self.STATUS_MODBUS = ERROR_CODE
						self.STATUS_CONN = 0x0
						self.CONN_ESTABLISHED = False
						self.thread_cmd_queue('QUIT')					


					
						
						
					#?self.thread_cmd_queue.put(ERROR_CODE)
									
					

		return self.STATUS

	def get_status(self) :
		return self.STATUS


	
