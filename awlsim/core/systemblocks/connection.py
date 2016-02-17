
from awlsim.core.systemblocks.connection_params import *
import math
import socket
import select
#import queue
#from queue import *
from multiprocessing import Queue as Queue
import time



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

modbus_area_data_types = {
		0 : "Unused",
		1 : "Coils",
		2 : "Inputs",
		3 : "Holding_Register",
		4 : "Input_Register",
	}	


class Connection(object) :

	
	def __init__(self,cpu,connection_id,remote_port,local_port,remote_host_name,is_server,single_write_enabled,data_areas):
		self.connection_params = Connection_Params(cpu)
		self.connection_params.set_connection_params(connection_id,remote_port,local_port,self.hostname_to_ip(remote_host_name),is_server,single_write_enabled)
		self.cpu = cpu
		
		self.data_area_dbs = {}
		self.thread_cmd_queue = Queue()
		self.thread_resp_queue = Queue()
		self.STATUS = NOT_STARTED
		self.is_server = is_server
		self.param_init_lock = threading.Lock()
		self.status_lock = threading.Lock()
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
		
	def hostname_to_ip(self,hostname) :

		host_id = int(hostname)
		conf_file = conf_directory + "/PLC_Config/" + str(host_id)
		lines = [line.rstrip('\n') for line in open(conf_file)]			
		for line in lines :
			line = ' '.join(line.split())
			line = line.split('=')
			if len(line) > 1 :
				print(line)
				parameter = line[0]
				value= line[1]
				if "lxc.network.ipv4" in parameter :
					value = value.replace(' ',"")
					value = value.split("/")
					print("Dest IP : ", value[0])
					resolved_hostname = value[0]
					break


		print("Resolved hostname = ",resolved_hostname)
		return resolved_hostname




	def set_connection_param_fields(self,db) :
		self.connection_params.set_connection_param_fields(self.cpu,db)	
		self.connection_params.store_connection_params(self.cpu,db)
		self.connection_params.load_connection_params(db)
		#self.connection_params.print_connection_params()

	def set_timeouts(self,recv_time,conn_time) :
		self.recv_time = recv_time
		self.conn_time = conn_time
		

	def run_server(self) :
		self.read_finish_status = 1
		TCP_LOCAL_PORT = self.connection_params.local_tsap_id
		BUFFER_SIZE = 4096
		self.BUSY = True
		server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		#server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		#server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)


		server_socket.settimeout(self.conn_time)
		local_id = str(self.cpu.local_id)
		server_host_ip = self.hostname_to_ip(local_id)
		server_socket.bind(('0.0.0.0',TCP_LOCAL_PORT))	# bind to any address
		server_socket.listen(1)

		print("Start time = ", time.time())
		print("Listening on port " + str(TCP_LOCAL_PORT))


		
		

		#print("Select succeeded");	
		
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
		self.thread_resp_queue.put(1)
		self.thread_cmd_queue.get()
		#client_socket.settimeout(self.recv_time)
		while True:
			
			
					
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

			recv_data = bytearray(data)
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

			#self.thread_resp_queue.put(1)
			#self.thread_cmd_queue.get()
			
			client_socket.send(response)
			print("Sent response to client = ",response)
			
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

	
	def run_client(self) :
		self.read_finish_status = 1
		TCP_REMOTE_IP  = self.connection_params.rem_staddr
		#TCP_REMOTE_IP = "10.100.0.1"
		TCP_REMOTE_PORT = self.connection_params.rem_tsap_id
		BUFFER_SIZE = 4096
		self.BUSY = True
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		#client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		#client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

		
		
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
		self.thread_resp_queue.put(1)
		self.thread_cmd_queue.get()
		
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

			client_socket.settimeout(self.recv_time)
			self.thread_resp_queue.put(1)
			msg_to_send,exception = self.mod_client.construct_request_message(data_type,write_read,length,single_write,start_address,TI)

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

			self.thread_resp_queue.put(1)
			self.thread_cmd_queue.get()
			
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
			recv_data = bytearray(data)
			print("Response from server : ",recv_data)
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

							
				threading.Thread(target=self.run_server).start()
				try :
					o = self.thread_resp_queue.get(block=True,timeout=0.1)
					self.thread_cmd_queue.put(1)
				except:
					pass
			self.PREV_ENQ_ENR = self.ENQ_ENR

		elif self.STATUS != RUNNING : 	# completed 
			if self.read_finish_status == 0 :
				self.read_finish_status = 1
				self.status_lock.release()

			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True :
				if self.ENQ_ENR == 1 :
					self.STATUS = RUNNING
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
					threading.Thread(target=self.run_server).start()
					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)
						self.thread_cmd_queue.put(1)
					except:
						pass
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.status_lock.release()
			try :
				o = self.thread_resp_queue.get(block=True,timeout=0.1)
				self.thread_cmd_queue.put(1)
			except:
				pass

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

				nsleep(5000000)
				
				threading.Thread(target=self.run_client).start()
				try :
					o = self.thread_resp_queue.get(block=True,timeout=0.1)
					self.thread_cmd_queue.put(1)
				except:
					pass
			self.PREV_ENQ_ENR = self.ENQ_ENR			

		elif self.STATUS != RUNNING  : 	# completed
			if self.read_finish_status == 0 :
				self.read_finish_status = 1
				self.status_lock.release()
			elif self.STATUS == DONE and self.CONN_ESTABLISHED == True :
				if self.ENQ_ENR == 1 and self.PREV_ENQ_ENR == 0 :
					self.STATUS = RUNNING
					self.thread_cmd_queue.put(1) # resume connection
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
					threading.Thread(target=self.run_client).start()
					try :
						o = self.thread_resp_queue.get(block=True,timeout=0.1)
						self.thread_cmd_queue.put(1)
					except:
						pass
					
				
			self.PREV_ENQ_ENR = self.ENQ_ENR
			return self.STATUS

		elif self.STATUS == RUNNING :
			self.PREV_ENQ_ENR = self.ENQ_ENR
			self.status_lock.release()
			try :
				o = self.thread_resp_queue.get(block=True,timeout=0.1)
				self.thread_cmd_queue.put(1)
			except:
				pass

		return self.STATUS

	def get_status(self) :
		return self.STATUS


	
