import math
import socket
import select
import sys
#import queue
#from queue import *
import multiprocessing, Queue
#from multiprocessing import Queue as Queue
import time
import os
from awlsim.core.systemblocks.exceptions import *
import select
import hashlib
import datetime
import random
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


#read_finish_status
#local_tsap_id
#ENQ_ENR, disconnect, recv_time, conn_time

def get_busy_wait(queue_name):
	o = None
	#while o == None :
	#	try :
	#		o = queue_name.get(block=False)		
	#	except Queue.Empty:
	#		o = None
	o = queue_name.get()

	return o	


def LOG_msg(msg,node_id):
	with open("/home/vignesh/Desktop/PLCs/awlsim-0.42/Projects/Bottle_Plant/conf/logs/node_" + str(node_id) + "_log","a") as f:
		f.write(msg + "\n")



def test_run_server_ip(thread_resp_queue,thread_cmd_queue,local_tsap_id,disconnect,recv_time_val,conn_time,IDS_IP,local_id):

	
	
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
		server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("ERRor creating socket")
		print(sockerror)
		return
	ids_host = IDS_IP;
	ids_port = 8888; 

	server_socket.settimeout(conn_time)
	try:
		server_socket.bind(('0.0.0.0',TCP_LOCAL_PORT))	# bind to any address
	except socket.error as sockerror:
		print("ERROR binding to socket")
		print(sockerror)
		return

	server_socket.listen(1)

	print("Start time = ", time.time())
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
		thread_resp_queue.put(curr_status)
		thread_cmd_queue.get()
		print("Sever exiting")
		return			
		
	print("Established Connection from " + str(address))
	CONN_ESTABLISHED = True
	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	read_finish_status = 1
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	thread_resp_queue.put(curr_status)		
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
		#?client_socket.settimeout(recv_time_val)

		
		try:
			data = client_socket.recv(BUFFER_SIZE)
			#time.sleep(0.01)
			
		except socket.timeout:
			print("RECV timeout error")
			read_finish_status = 0
			STATUS = SERVER_ERROR
			ERROR = True
			STATUS_MODBUS = ERROR_UNKNOWN_EXCEPTION
			STATUS_CONN = 0x0
			break

		


		#recv_time = time.time()
		recv_time = datetime.datetime.now()		
		recv_data = bytearray(data)
		if len(data) == 0 :
			#?client_socket.close()
			#?server_socket.close()
			#?return
			break

		

		# Should this be there ?
		#?thread_resp_queue.put(curr_status)		
		##cmd = thread_cmd_queue.get()
		#?cmd = get_busy_wait(thread_cmd_queue)
		#?if cmd == 'QUIT':
		#?	client_socket.close()
		#?	server_socket.close()
		#?	return

		
    	#Set the whole string
		recv_data_hash = str(hashlib.md5(str(recv_data)).hexdigest())
		log_msg = str(recv_time) + ",RECV," + str(recv_data_hash)
		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data_hash)
		
		try :			
			#ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id)
		except socket.error as sockerror :
			print(sockerror)
		
		print("Server: Recv new msg = ", ' '.join('{:02x}'.format(x) for x in recv_data), " at Node id = ", local_id + 1, " at " + str(datetime.datetime.now()))
		sys.stdout.flush()
		recv_data = recv_data[0:-3]
		thread_resp_queue.put((recv_data,1))
		##cmd = thread_cmd_queue.get()
		#?cmd = get_busy_wait(thread_cmd_queue)
		#?if cmd == 'QUIT':
		#?	client_socket.close()
		#?	server_socket.close()
		#?	return
		
		##cmd = thread_cmd_queue.get()
		cmd = get_busy_wait(thread_cmd_queue)
		print("Server : test run : received cmd at " + str(datetime.datetime.now()))
		if cmd == 'QUIT':
			client_socket.close()
			server_socket.close()
			return

		response = cmd[0]
		request_msg_params = cmd[1]

		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		response.append(random.randint(0,255))
		client_socket.send(response)

		print("Sent response to client = ",response, " at " + str(datetime.datetime.now()))
		sys.stdout.flush()
		response_hash = str(hashlib.md5(str(response)).hexdigest())
		log_msg = str(datetime.datetime.now())  + ",SEND," + str(response_hash)
		msg = str(local_id) + "," + str(datetime.datetime.now())  + ",SEND," + str(response_hash)
		try :		
			#ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id)
		except socket.error as sockerror :
			print(sockerror)

		if disconnect == True :
			client_socket.close()
			server_socket.close()
			return

		
		#?if disconnect == True :	
		#?	print("Disconnecting connection")		
		#?	read_finish_status = 0
		#?	STATUS = DONE
		#?	ERROR = False
		#?	break
		#?else :
		#?	read_finish_status = 0
		#?	STATUS = DONE
		#?	ERROR = False
		#?	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		#?	thread_resp_queue.put(curr_status)
		#?	##cmd = thread_cmd_queue.get()
		#?	cmd = get_busy_wait(thread_cmd_queue)
		#?	if cmd == 'QUIT' :
		#?		client_socket.close()
		#?		server_socket.close()
		#?		return

	print("####### Exiting ############")
	BUSY = False
	CONN_ESTABLISHED = False
	ids_socket.close()
	client_socket.close()
	server_socket.close()
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	thread_resp_queue.put(curr_status)
	thread_cmd_queue.get()


def test_run_client_ip(thread_resp_queue,thread_cmd_queue,local_tsap_id,IDS_IP,TCP_REMOTE_IP,TCP_REMOTE_PORT,conn_time, local_id) :
	
	BUFFER_SIZE = 4096	
	
	

	BUSY = True
	STATUS = RUNNING
	ERROR = False
	STATUS_MODBUS = 0x0
	STATUS_CONN = 0x0
	read_finish_status = 1

	
	try:
		ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("Error creating socket")
		print(sockerror)

	ids_host = str(IDS_IP);
	ids_port = 8888; 

	s_time = time.time()
	print("Start time = ", time.time())
	print("IP:PORT = ", TCP_REMOTE_IP,TCP_REMOTE_PORT)
	sys.stdout.flush()
	no_error = False
	attempt_no = 0
	
	while no_error == False:
		print("Attempting to connect to server ", TCP_REMOTE_IP, " : ", TCP_REMOTE_PORT, " for the ", attempt_no, " time.")
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			client_socket.settimeout(conn_time)
			client_socket.connect((TCP_REMOTE_IP,TCP_REMOTE_PORT))
			no_error = True
		except socket.error as socketerror:
			print("Client Error : ",TCP_REMOTE_IP,TCP_REMOTE_PORT, " ", socketerror, " at ", str(datetime.datetime.now()))
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
				thread_resp_queue.put(curr_status)
				cmd = thread_cmd_queue.get()
				if cmd == 'QUIT':
					client_socket.close()
					return
			else:
				nsleep(10000)

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
	
	read_finish_status = 1
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
	# read all input and inout params
	thread_resp_queue.put(curr_status)
	##cmd = thread_cmd_queue.get()
	cmd = get_busy_wait(thread_cmd_queue)
	if cmd == 'QUIT':
		client_socket.close()
		return
	
	while True :


		BUSY = True
		STATUS = RUNNING
		ERROR = False
		STATUS_MODBUS = 0x0
		STATUS_CONN = 0x0

		read_finish_status = 1
		curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		
		# read all input and inout params
		#?thread_resp_queue.put(curr_status)
		##cmd = thread_cmd_queue.get()
		#?cmd = get_busy_wait(thread_cmd_queue)
		#?if cmd == 'QUIT':
		#?	client_socket.close()
		#?	return

		print("Resumed connection at : " + str(datetime.datetime.now()))
		sys.stdout.flush()
		disconnect, recv_time_val, conn_time, msg_to_send, exception  = cmd
		#?client_socket.settimeout(recv_time_val)
		
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
		log_msg = str(datetime.datetime.now()) + ",SEND," + str(msg_to_send_hash)
		msg = str(local_id) + "," + str(datetime.datetime.now()) + ",SEND," + str(msg_to_send_hash)
		try :			
			#ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id)			
		except socket.error as sockerror :
			print(sockerror)

		#curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		#thread_resp_queue.put(curr_status)
		#cmd = thread_cmd_queue.get()
		#if cmd == 'QUIT':
		#	return
					
		try:
			data = client_socket.recv(BUFFER_SIZE)
			#time.sleep(0.01)
			
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
		#print("Response from server : ",recv_data, " at Node id = ", local_id + 1)
		print ("Response from server = ",' '.join('{:02x}'.format(x) for x in recv_data)," at ", str(datetime.datetime.now()))
		sys.stdout.flush()

		recv_data_hash = str(hashlib.md5(str(recv_data)).hexdigest())
		log_msg = str(recv_time) + ",RECV," + str(recv_data_hash)
		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data_hash)
		try :			
			#ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			LOG_msg(log_msg,local_id)
			#pass
		except socket.error as sockerror :
			print(sockerror)


		recv_data = recv_data[0:-3]
		thread_resp_queue.put((recv_data,1))
		##cmd = thread_cmd_queue.get()
		cmd = get_busy_wait(thread_cmd_queue)
		if cmd == 'QUIT':
			client_socket.close()
			return

		print("Response processed at " + str(datetime.datetime.now()))	
		sys.stdout.flush()

		#ERROR_CODE = cmd
		#if ERROR_CODE == NO_ERROR :
		#	ERROR = False
		#else :
			# set STATUS_MODBUS Based on returned error value
		#	read_finish_status = 0
		#	STATUS = DONE
		#	ERROR = True
		#	STATUS_MODBUS = ERROR_CODE
		#	STATUS_CONN = 0x0
		#	break

		#if disconnect == True :
		#	read_finish_status = 0
		#	STATUS = DONE
		#	ERROR = False
		#	break
		#else :
		#	read_finish_status = 0
		#	STATUS = DONE
		#	ERROR = False
		#	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
		#	thread_resp_queue.put(curr_status)
		#	##cmd = thread_cmd_queue.get()
		#	cmd = get_busy_wait(thread_cmd_queue)
		#	if cmd == 'QUIT':
		#		client_socket.close()
		#		return
				
				
	BUSY = False
	CONN_ESTABLISHED = False
	client_socket.close()
	ids_socket.close()
	curr_status = (read_finish_status,STATUS,ERROR,STATUS_MODBUS,STATUS_CONN,BUSY,CONN_ESTABLISHED)
	thread_resp_queue.put(curr_status)
	cmd = thread_cmd_queue.get()
	
