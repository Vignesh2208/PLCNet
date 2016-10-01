import math
import socket
import select
import sys
import multiprocessing, Queue
from multiprocessing import Array as Array
import binascii
import time
import os
from awlsim.core.systemblocks.exceptions import *
import select
import hashlib
import datetime
import random
import ctypes
import set_connection
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




#read_finish_status
#local_tsap_id
#ENQ_ENR, disconnect, recv_time, conn_time
def put(shared_Array,type_of_data,data):
	
	shared_Array[0] = 0 # Data is not available to read
	shared_Array[1] = int(type_of_data)
	if type_of_data == 1:	# data is a byte array
		data_len = len(data)
		shared_Array[2] = int(data_len)
		i = 3
		for x in data :
			shared_Array[i] = int(x)
			i = i + 1

	if type_of_data == 2 : # data is a tuple (disconnect, recv_time_val, conn_time, msg_to_send, exception)
		if data[0] == True :
			disconnect = 1
		else :
			disconnect = 0
		recv_time_val = int(data[1])
		conn_time = int(data[2])
		msg_to_send = data[3]
		data_len = len(msg_to_send)
		exception = int(data[4])

		shared_Array[2] = disconnect
		shared_Array[3] = recv_time_val
		shared_Array[4] = conn_time
		shared_Array[5] = exception
		shared_Array[6] = data_len
		i = 7
		for x in msg_to_send :
			shared_Array[i] = int(x)
			i = i + 1

	if type_of_data == 4 :	# data is a status tuple (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		#self.STATUS = -1
		#self.ERROR = False
		#self.STATUS_MODBUS = 0x0000
		#self.STATUS_CONN = 0x0000
		#self.STATUS_FUNC = "MODBUSPN"
		#self.IDENT_CODE = "NONE"
		#self.CONN_ESTABLISHED = False
		#self.BUSY = False
		i = 2
		for x in data:
			shared_Array[i] = int(x)
			i = i + 1



	shared_Array[0] = 1	# Data is available to read

def get(shared_Array,block=True):
	ret = None
	if block == True :
		while shared_Array[0] == 0 :
			pass
	elif shared_Array[0] == 0 :
		return None 

	type_of_data = shared_Array[1]

	if type_of_data == 4 :
		read_finish_status = int(shared_Array[2])
		STATUS = int(shared_Array[3])
		ERROR = bool(shared_Array[4])
		STATUS_MODBUS = int(shared_Array[5])
		STATUS_CONN = int(shared_Array[6])
		BUSY = bool(shared_Array[7])
		CONN_ESTABLISHED = bool(shared_Array[8])

		ret = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)

	if type_of_data == 3 :			
		ret = 'QUIT'

	if type_of_data == 1 : # data is a byte array
		data_len = shared_Array[2]
		ret = bytearray()
		i = 0
		while i < data_len :
			ret.append(shared_Array[3+i])
			i = i + 1
	if type_of_data == 2 : # data is a typle
		disconnect = shared_Array[2]
		if disconnect == 1 :
			disconnect = True 
		else :
			disconnect = False

		recv_time_val = shared_Array[3]
		conn_time = shared_Array[4]
		exception = shared_Array[5]
		data_len = shared_Array[6]
		msg_to_send = bytearray()
		j = 0
		while j < data_len :
			msg_to_send.append(shared_Array[7+j])
			j = j + 1

		ret = (disconnect,recv_time_val,conn_time,msg_to_send,exception)
		

	shared_Array[0] = 0
	return ret


def get_busy_wait(queue_name):
	#o = None
	#while o == None :
	#	try :
	#		o = queue_name.get(block=False)		
	#	except Queue.Empty:
	#		o = None
	#o = queue_name.get()

	return get(queue_name)


def LOG_msg(msg,node_id,conf_directory):
	fname = conf_directory + "/logs/node_" + str(node_id) + "_log"
	with open(fname,"a") as f:
		f.write(msg + "\n")



def test_run_server_ip(thread_resp_queue,thread_cmd_queue,local_tsap_id,disconnect,recv_time_val,conn_time,IDS_IP,local_id,thread_resp_arr,thread_cmd_arr,conf_directory):

	
	
	TCP_LOCAL_PORT = local_tsap_id
	BUFFER_SIZE = 4096
	
	# init status
	read_finish_status = 1
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	
	server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	
		
	try:
		ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		#server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("ERRor creating socket")
		print(sockerror)
		return
	ids_host = IDS_IP;
	ids_port = 8888; 

	server_socket.settimeout(conn_time)
	try:
		server_socket.bind(('',TCP_LOCAL_PORT))	# bind to any address
	except socket.error as sockerror:
		print("ERROR binding to socket")
		print(sockerror)
		return

	server_socket.listen(1)

	print("Start time = ", str(datetime.datetime.now()))
	print("Listening on port " + str(TCP_LOCAL_PORT))	
	sys.stdout.flush()			
	
	try:
		client_socket, address = server_socket.accept()
	except socket.timeout:
		print("End time = ", time.time())
		print("Socket TIMEOUT Done !!!!!!!!!!!!!!!!")
		read_finish_status = 0
		STATUS = CONN_TIMEOUT_ERROR
		ERROR = True
		STATUS_MODBUS = 0x0
		STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
		BUSY = False
		CONN_ESTABLISHED = False
		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		put(thread_resp_queue,4,curr_status)
		get(thread_cmd_queue)
		print("Sever exiting")
		return			
		
	print("Established Connection from " + str(address) + " at " + str(datetime.datetime.now()))
	sys.stdout.flush()
	CONN_ESTABLISHED = True
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	read_finish_status = 1
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	put(thread_resp_queue,4,curr_status)		
	cmd = get_busy_wait(thread_cmd_queue)

	if cmd == 'QUIT':
		client_socket.close()
		server_socket.close()
		return

	while True:
		
		BUSY = True
		STATUS = RUNNING
		ERROR = False
		STATUS_MODBUS = 0x0
		STATUS_CONN = 0x0

		read_finish_status = 1
		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		client_socket.settimeout(recv_time_val)

		
		try:
			data = client_socket.recv(BUFFER_SIZE)
			
			
		except socket.timeout:
			print("RECV timeout error")
			read_finish_status = 0
			STATUS = SERVER_ERROR
			ERROR = True
			STATUS_MODBUS = ERROR_UNKNOWN_EXCEPTION
			STATUS_CONN = 0x0
			break

		


	
		recv_time = datetime.datetime.now()		
		recv_data = bytearray(data)
		if len(data) == 0 :
			break

		
    		#Set the whole string
		recv_data_hash = str(hashlib.md5(str(recv_data)).hexdigest())
		log_msg = str(recv_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_data[0:-3]))
		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_data[0:-3]))
		
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id,conf_directory)
		except socket.error as sockerror :
			print(sockerror)
		
		print("Server: Recv new msg = ", ' '.join('{:02x}'.format(x) for x in recv_data), " at Node id = ", local_id + 1, " at " + str(datetime.datetime.now()))
		sys.stdout.flush()



		recv_data = recv_data[0:-3]
		put(thread_resp_arr,1,recv_data)
		cmd = get(thread_cmd_arr)

		print("Server : test run : received cmd at " + str(datetime.datetime.now()))
		if cmd == 'QUIT':
			client_socket.close()
			server_socket.close()
			return

		
		response = cmd
		
		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		client_socket.send(response)

		print("Sent response to client = ",response, " at " + str(datetime.datetime.now()))
		sys.stdout.flush()
		response_hash = str(hashlib.md5(str(response)).hexdigest())
		log_msg = str(datetime.datetime.now())  + ",SEND," + str(response_hash) + "," + str(binascii.hexlify(response[0:-3]))
		msg = str(local_id) + "," + str(datetime.datetime.now())  + ",SEND," + str(response_hash) + "," + str(binascii.hexlify(response[0:-3]))
		try :		
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id,conf_directory)
		except socket.error as sockerror :
			print(sockerror)

		if disconnect == True :
			client_socket.close()
			server_socket.close()
			return		
		

	print("####### Exiting IP Server ############")
	BUSY = False
	CONN_ESTABLISHED = False
	ids_socket.close()
	client_socket.close()
	server_socket.close()
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	put(thread_resp_queue,4,curr_status)
	get(thread_cmd_queue)


def test_run_client_ip(thread_resp_queue,thread_cmd_queue,local_tsap_id,IDS_IP,TCP_REMOTE_IP,TCP_REMOTE_PORT,conn_time, local_id,thread_resp_arr,thread_cmd_arr,conf_directory) :
	
	BUFFER_SIZE = 4096
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	read_finish_status = 1

	#time.sleep(1)

	
	try:
		ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("Error creating socket")
		print(sockerror)

	ids_host = str(IDS_IP);
	ids_port = 8888; 

	s_time = time.time()
	print("Start time = ", str(datetime.datetime.now()))
	print("IP:PORT = ", TCP_REMOTE_IP,TCP_REMOTE_PORT)
	sys.stdout.flush()
	no_error = False
	attempt_no = 0
	
	while no_error == False:
		print("Attempting to connect to server ", TCP_REMOTE_IP, " : ", TCP_REMOTE_PORT, " for the ", attempt_no, " time at: ", str(datetime.datetime.now()))
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		sys.stdout.flush()
		try:
			
			client_socket.settimeout(conn_time)
			client_socket.connect((TCP_REMOTE_IP,TCP_REMOTE_PORT))
			
			no_error = True
		except socket.error as socketerror:
			print("Client Error : ",TCP_REMOTE_IP,TCP_REMOTE_PORT, " ", socketerror, " at ", str(datetime.datetime.now()))
			sys.stdout.flush()
			read_finish_status = 0
			STATUS = CONN_TIMEOUT_ERROR
			ERROR = True
			STATUS_MODBUS = 0x0
			STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
			BUSY = False
			CONN_ESTABLISHED = False
			curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
			no_error = False
			if time.time() - s_time > conn_time :
				put(thread_resp_queue,4,curr_status)
				cmd = get(thread_cmd_queue)
				if cmd == 'QUIT':
					client_socket.close()
					return
			else:
				#nsleep(10000)
				time.sleep(0.01)

			client_socket.close()
			#return
		attempt_no = attempt_no + 1

	CONN_ESTABLISHED = True
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0

	print("Connection established at " + str(datetime.datetime.now()))
	sys.stdout.flush()
	
	read_finish_status = 1
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
	# read all input and inout params
	put(thread_resp_queue,4,curr_status)
	cmd = get_busy_wait(thread_cmd_queue)
	

	if cmd == 'QUIT':
		client_socket.close()
		return

	disconnect, recv_time_val, conn_time, msg_to_send, exception  = cmd
	
	while True :


		BUSY = True
		STATUS = RUNNING
		ERROR = False
		STATUS_MODBUS = 0x0
		STATUS_CONN = 0x0

		read_finish_status = 1
		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
	
		print("Resumed connection at : " + str(datetime.datetime.now()))
		sys.stdout.flush()
		client_socket.settimeout(recv_time_val)		
		if msg_to_send == None :
			read_finish_status = 0
			STATUS = DONE
			ERROR = True
			STATUS_MODBUS = exception
			STATUS_CONN = 0x0
			break

		msg_to_send.append(random.randint(0,255))
		msg_to_send.append(random.randint(0,255))
		msg_to_send.append(random.randint(0,255))
		
		try:
			
			print("Sent msg = ",msg_to_send)
			client_socket.send(msg_to_send)
			sys.stdout.flush()
		except socket.error as socketerror :
			print("Client Error : ", socketerror)
			read_finish_status = 0
			STATUS = CLIENT_ERROR
			ERROR = True 
			STATUS_MODBUS = ERROR_UNKNOWN_EXCEPTION
			STATUS_CONN = 0x0
			break

		msg_to_send_hash = str(hashlib.md5(str(msg_to_send)).hexdigest())
		log_msg = str(datetime.datetime.now()) + ",SEND," + str(msg_to_send_hash) + "," + str(binascii.hexlify(msg_to_send[0:-3]))
		msg = str(local_id) + "," + str(datetime.datetime.now()) + ",SEND," + str(msg_to_send_hash) + "," + str(binascii.hexlify(msg_to_send[0:-3]))
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id,conf_directory)			
		except socket.error as sockerror :
			print(sockerror)

		
					
		try:
			data = client_socket.recv(BUFFER_SIZE)
			
			
		except socket.timeout:
			print("Client RECV timeout error")
			read_finish_status = 0
			STATUS = RECV_TIMEOUT_ERROR
			ERROR = True
			STATUS_MODBUS = 0x0
			STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
			break

		recv_time = datetime.datetime.now()
		recv_data = bytearray(data)
		print ("Response from server = ",' '.join('{:02x}'.format(x) for x in recv_data)," at ", str(datetime.datetime.now()))
		sys.stdout.flush()

		recv_data_hash = str(hashlib.md5(str(recv_data)).hexdigest())
		log_msg = str(recv_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_data[0:-3]))
		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_data[0:-3]))
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id,conf_directory)
		except socket.error as sockerror :
			print(sockerror)



		recv_data = recv_data[0:-3]
		put(thread_resp_arr,1,recv_data)
		cmd = get(thread_cmd_arr)


		
		if cmd == 'QUIT':
			client_socket.close()
			return

		print("Response processed at " + str(datetime.datetime.now()))	
		sys.stdout.flush()
		disconnect, recv_time_val, conn_time, msg_to_send, exception  = cmd

		
				
	print("####### Exiting IP Client ############")			
	BUSY = False
	CONN_ESTABLISHED = False
	client_socket.close()
	ids_socket.close()
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	put(thread_resp_queue,4,curr_status)
	cmd = get(thread_cmd_queue)



START_END_FLAG = 0x7E
ESCAPE_FLAG = 0x7D


def frame_outgoing_message(msg) :

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

def process_incoming_frame(msg) :

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


def test_run_client_serial(thread_resp_queue,thread_cmd_queue,conn_time, local_id, remote_id, connection_id, thread_resp_arr,thread_cmd_arr,conf_directory) :
	
	BUFFER_SIZE = 4096
	read_finish_status = 1

	s_time = time.time()
	print("Start time = ", datetime.datetime.now()," connection id = ", connection_id)
	client_fd = os.open("/dev/s3fserial" + str(connection_id) ,os.O_RDWR)

	READ_ONLY = select.POLLIN 
	WRITE_ONLY = select.POLLOUT		
	set_connection.set_connection(local_id,remote_id,connection_id)
	sys.stdout.flush()

	
	CONN_ESTABLISHED = True
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0

	print("Connection established at " + str(datetime.datetime.now()))
	
	read_finish_status = 1
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
	# read all input and inout params
	put(thread_resp_queue,4,curr_status)
	cmd = get_busy_wait(thread_cmd_queue)
	

	if cmd == 'QUIT':
		return

	disconnect, recv_time_val, conn_time, msg_to_send, exception  = cmd
	
	while True :


		BUSY = True
		STATUS = RUNNING
		ERROR = False
		STATUS_MODBUS = 0x0
		STATUS_CONN = 0x0

		read_finish_status = 1
		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
	
		print("Resumed connection at : " + str(datetime.datetime.now()))
		sys.stdout.flush()
		
		if msg_to_send == None :
			read_finish_status = 0
			STATUS = DONE
			ERROR = True
			STATUS_MODBUS = exception
			STATUS_CONN = 0x0
			break

		msg_to_send.append(random.randint(0,255))
		msg_to_send.append(random.randint(0,255))
		msg_to_send.append(random.randint(0,255))

		msg_to_send = frame_outgoing_message(msg_to_send)

		n_wrote = 0
		wt_start = datetime.datetime.now()
		while n_wrote < len(msg_to_send) :
			poller = select.poll()
			poller.register(client_fd, WRITE_ONLY)
			events = poller.poll(conn_time*1000)
			log_time = None
			if len(events) == 0 :
				print("End time = ", time.time())
				print("Serial Write TIMEOUT Done !!!!!!!!!!!!!!!!")
				read_finish_status = 0
				STATUS = CONN_TIMEOUT_ERROR
				ERROR = True
				STATUS_MODBUS = 0x0
				STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
				break

			n_wrote = n_wrote + os.write(client_fd,msg_to_send[n_wrote:])

		
		wt_end = datetime.datetime.now()
		d = wt_end - wt_start
		#print(" Client send msg write time = ", d.total_seconds())	

		if log_time == None :
			log_time = datetime.datetime.now()

		msg_to_send_hash = str(hashlib.md5(str(msg_to_send)).hexdigest())
		log_msg = str(log_time) + ",SEND," + str(msg_to_send_hash) + "," + str(binascii.hexlify(msg_to_send[0:-3]))
		LOG_msg(log_msg,local_id,conf_directory)
		

		# receive response from server
		recv_msg = bytearray()
		isfirst = 1
		resume_poll = True
		data = []
		elapsed = 0.0
		number = 0

		while True :
			poller = select.poll()
			poller.register(client_fd, READ_ONLY)
			events = poller.poll(recv_time_val*1000)
			if isfirst == 1 :
				log_time = datetime.datetime.now()
				isfirst = 0
			
			if len(events) == 0 :
				print("End time = ", datetime.datetime.now())
				print("Serial Read TIMEOUT Done !!!!!!!!!!!!!!!!")
				read_finish_status = 0
				STATUS = RECV_TIMEOUT_ERROR
				ERROR = True
				STATUS_MODBUS = 0x0
				STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
				BUSY = False
				break


			st = datetime.datetime.now()
			data = os.read(client_fd,100)
			end = datetime.datetime.now()
			d = end - st
			elapsed = elapsed + d.total_seconds()
			number = number + 1.0
			if len(data) == 0 :
				continue
			else :
				if isfirst == 1 :
					
					log_time = datetime.datetime.now()
					#print("Recv time = ",time.time(), " last elapsed = ", d.total_seconds())
					isfirst = 0

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
		
		
		recv_time = log_time
		recv_data_hash = str(hashlib.md5(str(recv_msg)).hexdigest())
		log_msg = str(recv_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_msg[0:-3]))
		#print("Recv msg = ",recv_msg, " Avg read time = ", float(elapsed)/float(number), " Number = ", number, " total read time = ", elapsed)
		LOG_msg(log_msg,local_id,conf_directory)				
		recv_data = process_incoming_frame(recv_msg)
		recv_data = recv_data[0:-3]

		
		put(thread_resp_arr,1,recv_data)
		cmd = get(thread_cmd_arr)

		
		if cmd == 'QUIT':
			return

		print("Response processed at " + str(datetime.datetime.now()))	
		sys.stdout.flush()
		disconnect, recv_time_val, conn_time, msg_to_send, exception  = cmd
		elapsed = 0.0
		number = 0.0

		
				
	print("####### Exiting Serial Client ############")				
	BUSY = False
	CONN_ESTABLISHED = False
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	put(thread_resp_queue,4,curr_status)
	cmd = get(thread_cmd_queue)
	


def test_run_server_serial(thread_resp_queue,thread_cmd_queue, disconnect, recv_time_val, conn_time, local_id, remote_id, connection_id, thread_resp_arr,thread_cmd_arr,conf_directory):

	
	
	BUFFER_SIZE = 4096	
	# init status
	read_finish_status = 1
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	server_fd = os.open("/dev/s3fserial" + str(connection_id) ,os.O_RDWR)
	READ_ONLY = select.POLLIN 
	WRITE_ONLY = select.POLLOUT		
		
	print("Start time = ", datetime.datetime.now()," connection id = ", connection_id)
	set_connection.set_connection(local_id,remote_id,connection_id)
	recv_msg = bytearray()
	isfirst = 1
	log_time = None
	CONN_ESTABLISHED = False	
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	read_finish_status = 1
	elapsed = 0.0
	number = 0.0
	while True:
	
		


		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)				
		poller = select.poll()
		poller.register(server_fd, READ_ONLY)
		print ("poll start time :", datetime.datetime.now())
		events = poller.poll(recv_time_val*1000)
		if isfirst == 1 :
			log_time = datetime.datetime.now()
			isfirst = 0
		if len(events) == 0 :
			print("End time = ", datetime.datetime.now())
			print("Serial Recv TIMEOUT Done !!!!!!!!!!!!!!!!")
			read_finish_status = 0
			STATUS = RECV_TIMEOUT_ERROR
			ERROR = True
			STATUS_MODBUS = 0x0
			STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED			
			break

	
		
		st = datetime.datetime.now()
		data = os.read(server_fd,100)
		end = datetime.datetime.now()
		d = end - st
		elapsed = elapsed + d.total_seconds()
		number = number + 1.0
		if len(data) == 0 :
				continue
		else :
			if isfirst == 1 :
				
				log_time = datetime.datetime.now()
				#print("Recv time = ", time.time(), " last elapsed = ", d.total_seconds())
				isfirst = 0

		recv_msg.extend(data)
		data = bytearray(data)
		if data[len(data) - 1] == START_END_FLAG :
			resume_poll = False
		else :
			resume_poll = True
			

		if resume_poll == True :
			continue				

		if log_time == None :
			log_time = datetime.datetime.now()
			

		#print("Recv msg = ",recv_msg)
		recv_data_hash = str(hashlib.md5(str(recv_msg)).hexdigest())
		log_msg = str(log_time) + ",RECV," + str(recv_data_hash) + "," + str(binascii.hexlify(recv_msg[0:-3]))
		LOG_msg(log_msg,local_id,conf_directory)
		
		recv_data = process_incoming_frame(recv_msg)
		#print("Recv data = ",recv_data, " Avg read time = ", float(elapsed)/float(number), " Number = ", number, " total read time = ", elapsed)
		recv_data = recv_data[0:-3]
		
		put(thread_resp_arr,1,recv_data)
		cmd = get(thread_cmd_arr)

		#print("Server : test run : received cmd at " + str(datetime.datetime.now()))
		if cmd == 'QUIT':
			return		
		response = cmd		
		log_time = None
		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		response = frame_outgoing_message(response)
		n_wrote = 0
		wt_start = datetime.datetime.now()
		while n_wrote < len(response) :
			poller = select.poll()
			poller.register(server_fd, WRITE_ONLY)
			events = poller.poll(conn_time*1000)
			if len(events) == 0 :
				print("End time = ", time.time())
				print("Serial Send TIMEOUT Done !!!!!!!!!!!!!!!!")
				read_finish_status = 0
				STATUS = CONN_TIMEOUT_ERROR
				ERROR = True
				STATUS_MODBUS = 0x0
				STATUS_CONN = ERROR_MONITORING_TIME_ELAPSED
				break
			

			n_wrote = n_wrote + os.write(server_fd,response[n_wrote:])

		wt_end = datetime.datetime.now()
		d = wt_end - wt_start
		#print("Sent response to client = ",response, " at " + str(datetime.datetime.now()), " Write time = ", d.total_seconds())
		sys.stdout.flush()
		response_hash = str(hashlib.md5(str(response)).hexdigest())
		if log_time == None :
			log_time = datetime.datetime.now()

		log_msg = str(log_time)  + ",SEND," + str(response_hash) + "," + str(binascii.hexlify(response[0:-3]))
		LOG_msg(log_msg,local_id,conf_directory)		

		if disconnect == True :
			return

		isfirst = 1
		log_time = None
		recv_msg = bytearray()
		CONN_ESTABLISHED = False	
		BUSY = True
		STATUS = RUNNING
		ERROR = False
		STATUS_MODBUS = 0x0
		STATUS_CONN = 0x0
		read_finish_status = 1
		elapsed = 0.0
		number = 0.0
		

	print("####### Exiting Serial Server ############")
	BUSY = False
	CONN_ESTABLISHED = False
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	put(thread_resp_queue,4,curr_status)
	get(thread_cmd_queue)
