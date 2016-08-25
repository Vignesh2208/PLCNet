import math
import socket
import select
import sys
#import queue
#from queue import *
from multiprocessing import Queue as Queue
import time
import os

def run_server_ip(status_lock,param_init_lock,thread_resp_queue,thread_cmd_queue,conn_obj) :

	
	conn_obj.set_read_finish_status(1)
	TCP_LOCAL_PORT = conn_obj.get_local_tsap_id()
	BUFFER_SIZE = 4096
	conn_obj.set_BUSY(True)
	server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	ENQ_ENR, disconnect, recv_time, conn_time = conn_obj.get_in_parameters()
	mod_server = conn_obj.get_mod_server_client()

	
	try:
		ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("ERRor creating socket")
		print(sockerror)
	ids_host = str(conn_obj.get_IDS_IP());
	ids_port = 8888; 

	server_socket.settimeout(conn_time)
	local_id = str(conn_obj.get_local_host_id())
	server_host_ip = conn_obj.hostname_to_ip(local_id)
	server_socket.bind(('0.0.0.0',TCP_LOCAL_PORT))	# bind to any address
	server_socket.listen(1)

	print("Start time = ", time.time())
	print("Listening on port " + str(TCP_LOCAL_PORT))	
			
	
	try:
		client_socket, address = server_socket.accept()
	except socket.timeout:
		print("End time = ", time.time())
		print("Socket TIMEOUT Done !!!!!!!!!!!!!!!!")
		status_lock.acquire()
		conn_obj.set_read_finish_status(0)
		conn_obj.set_STATUS(CONN_TIMEOUT_ERROR)
		conn_obj.set_ERROR(True)
		conn_obj.set_STATUS_MODBUS(0x0)
		conn_obj.set_STATUS_CONN(ERROR_MONITORING_TIME_ELAPSED)
		conn_obj.set_BUSY(False)
		conn_obj.set_CONN_ESTABLISHED(False)
		thread_resp_queue.put(1)
		status_lock.release()
		return			
		
	print("Established Connection from " + str(address))
	conn_obj.set_CONN_ESTABLISHED(True)
	while True:
		
		thread_resp_queue.put(1)
		thread_cmd_queue.get()	
				
		try:
			data = client_socket.recv(BUFFER_SIZE)
		except socket.timeout:

			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(SERVER_ERROR)
			conn_obj.set_ERROR(True)
			conn_obj.set_STATUS_MODBUS(ERROR_UNKNOWN_EXCEPTION)
			conn_obj.set_STATUS_CONN(0x0)
			thread_resp_queue.put(1)
			break

		recv_time = time.time()
		recv_data = bytearray(data)
		print("Recv data = ",recv_data)	
	
    	#Set the whole string
		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data)
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			pass
		except socket.error as sockerror :
			print(sockerror)
		response,request_msg_params = mod_server.process_request_message(recv_data)
		

		if request_msg_params["ERROR"] == NO_ERROR :
			conn_obj.set_ERROR(False)
		else :
			# set STATUS_MODBUS Based on returned error value
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(True)
			conn_obj.set_STATUS_MODBUS(request_msg_params["ERROR"])
			conn_obj.set_STATUS_CONN(0x0)
			thread_resp_queue.put(1)
			break


		# write all inout params
		param_init_lock.acquire()
		conn_obj.set_inout_parameters(request_msg_params["unit"],request_msg_params["data_type"],request_msg_params["start_address"],request_msg_params["length"],request_msg_params["ti"],request_msg_params["write_read"])
		param_init_lock.release()

		client_socket.send(response)
		print("Sent response to client = ",response)
		msg = str(local_id) + "," + str(time.time())  + ",SEND," + str(response)
		try :		
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
		except socket.error as sockerror :
			print(sockerror)


		
		if disconnect == True :
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(False)
			thread_resp_queue.put(1)
			client_socket.close()
			break
		else :
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(False)
			status_lock.release()
			thread_resp_queue.put(1)
			thread_cmd_queue.get()
	conn_obj.set_BUSY(False)
	conn_obj.set_CONN_ESTABLISHED(False)
	status_lock.release()


	
def run_client_ip(status_lock,param_init_lock,thread_resp_queue,thread_cmd_queue,conn_obj) :
	conn_obj.set_read_finish_status(1)

	TCP_REMOTE_IP, TCP_REMOTE_PORT = conn_obj.get_remote_port_IP()
	BUFFER_SIZE = 4096
	conn_obj.set_BUSY(True)
	client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	ENQ_ENR, disconnect, recv_time, conn_time = conn_obj.get_in_parameters()
	mod_client = conn_obj.get_mod_server_client()
	remote_host_id = conn_obj.get_remote_host_id()

	try:
		ids_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		ids_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	except socket.error as sockerror:
		print("Error creating socket")
		print(sockerror)
	ids_host = str(conn_obj.get_IDS_IP());
	ids_port = 8888; 


	print("Start time = ", time.time())
	
	try:
		client_socket.settimeout(conn_time)
		client_socket.connect((TCP_REMOTE_IP,TCP_REMOTE_PORT))
	except socket.error as socketerror:
		print(TCP_REMOTE_IP,TCP_REMOTE_PORT)
		print(socketerror)
		status_lock.acquire()
		conn_obj.set_read_finish_status(0)
		conn_obj.set_STATUS(CONN_TIMEOUT_ERROR)
		conn_obj.set_ERROR(True)
		conn_obj.set_STATUS_MODBUS(0x0)
		conn_obj.set_STATUS_CONN(ERROR_MONITORING_TIME_ELAPSED)
		conn_obj.set_BUSY(False)
		conn_obj.set_CONN_ESTABLISHED(False)
		status_lock.release()
		thread_resp_queue.put(1)
		return

	conn_obj.set_CONN_ESTABLISHED(True)
	##thread_resp_queue.put(1)
	##thread_cmd_queue.get()
	
	while True :

		# read all input and inout params
		thread_resp_queue.put(1)
		thread_cmd_queue.get()

		print("Resumed connection")
		sys.stdout.flush()

		param_init_lock.acquire()

		unit, data_type, start_address, length, TI, write_read = conn_obj.get_inout_parameters()
		ENQ_ENR, disconnect, recv_time, conn_time = conn_obj.get_in_parameters()
		single_write = conn_obj.get_single_write()

		param_init_lock.release()

		client_socket.settimeout(recv_time)

		msg_to_send,exception = mod_client.construct_request_message(data_type,write_read,length,single_write,start_address,TI,remote_host_id)

		if msg_to_send == None :
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(True)
			conn_obj.set_STATUS_MODBUS(exception)
			conn_obj.set_STATUS_CONN(0x0)
			thread_resp_queue.put(1)
			break
		
		try:
			print("Sent msg = ",msg_to_send)
			client_socket.send(msg_to_send)
		except socket.error as socketerror :
			print("Client Error : ", socketerror)
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(CLIENT_ERROR)
			conn_obj.set_ERROR(True)
			conn_obj.set_STATUS_MODBUS(ERROR_UNKNOWN_EXCEPTION)
			conn_obj.set_STATUS_CONN(0x0)
			thread_resp_queue.put(1)
			break
		msg = str(local_id) + "," + str(time.time()) + ",SEND," + str(msg_to_send)
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			pass
		except socket.error as sockerror :
			print(sockerror)
		print("Waiting for cmd queue get")
		thread_resp_queue.put(1)
		thread_cmd_queue.get()
		print("Resumed from cmd queue get")
			
		try:
			data = client_socket.recv(BUFFER_SIZE)
		except socket.timeout:
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(RECV_TIMEOUT_ERROR)
			conn_obj.set_ERROR(True)
			conn_obj.set_STATUS_MODBUS(0x0)
			conn_obj.set_STATUS_CONN(ERROR_MONITORING_TIME_ELAPSED)
			thread_resp_queue.put(1)
			break
		recv_time = time.time()
		recv_data = bytearray(data)
		print("Response from server : ",recv_data)
			

		msg = str(local_id) + "," + str(recv_time) + ",RECV," + str(recv_data)
		try :			
			ids_socket.sendto(msg.encode('utf-8'), (ids_host, ids_port))
			pass
		except socket.error as sockerror :
			print(sockerror)

		ERROR_CODE = mod_client.process_response_message(recv_data)
		if ERROR_CODE == NO_ERROR :
			conn_obj.set_ERROR(False)
		else :
				# set STATUS_MODBUS Based on returned error value
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(True)					
			conn_obj.set_STATUS_MODBUS(ERROR_CODE)
			conn_obj.set_STATUS_CONN(0x0)
			thread_resp_queue.put(1)
			break

		if disconnect == True :
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(False)
			thread_resp_queue.put(1)
			client_socket.close()
			break
		else :
			status_lock.acquire()
			conn_obj.set_read_finish_status(0)
			conn_obj.set_STATUS(DONE)
			conn_obj.set_ERROR(False)
			status_lock.release()
			thread_resp_queue.put(1)
			thread_cmd_queue.get()
				
				
	conn_obj.set_BUSY(False)
	conn_obj.set_CONN_ESTABLISHED(False)
	status_lock.release()