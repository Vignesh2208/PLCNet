import os 
import sys
import time
import datetime
import math
import socket
import select
import random

def hostname_to_ip(hostConfigFile) :

	
	conf_file = hostConfigFile

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
					resolved_host_ip = value[0]
					break
	else:
		resolved_host_ip = "127.0.0.1"


	print("Resolved host IP  = ",resolved_host_ip)

	return resolved_host_ip


def connect_to_slave(SLAVE_IP, SLAVE_PORT,connect_timeout) :
	s_time = time.time()
	print("Start time = ", time.time())
	print("IP:PORT = ", SLAVE_IP,SLAVE_PORT)
	sys.stdout.flush()
	no_error = False
	attempt_no = 0


	while no_error == False:
		print("Attempting to connect to server ", SLAVE_IP, " : ", SLAVE_PORT, " for the ", attempt_no, " time.")
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			client_socket.settimeout(connect_timeout)
			client_socket.connect((SLAVE_IP,SLAVE_PORT))
			no_error = True
		except socket.error as socketerror:
			print("Client Error : ",SLAVE_IP,SLAVE_PORT, " ", socketerror, " at ", str(datetime.datetime.now()))			
			if time.time() - s_time > connect_timeout :
				print("Conn time expired. Quitting")
				sys.exit()
			else:
				time.sleep(0.1)
			client_socket.close()		
		attempt_no = attempt_no + 1

	print("Connection established at " + str(datetime.datetime.now()))
	return client_socket

def send_command_to_slave(client_socket, msg_to_send,timeout) :
	client_socket.settimeout(timeout)
	if msg_to_send == None :
		print "Msg to send is None"
		sys.exit()

	msg_to_send.append(random.randint(0,255))
	msg_to_send.append(random.randint(0,255))
	msg_to_send.append(random.randint(0,255))
		
	try:
		print("Sent msg = ",msg_to_send)
		client_socket.send(msg_to_send)
		sys.stdout.flush()
	except socket.error as socketerror :
		print("Client Error : ", socketerror)
		sys.exit()

	

def get_response_from_slave(client_socket,timeout,RECV_BUFFER_SIZE) :

	print "Waiting for response ..."
	client_socket.settimeout(timeout)
	try:
		data = client_socket.recv(RECV_BUFFER_SIZE)
	except socket.timeout:
		print "Recv timeout"
		sys.exit()
		

	recv_time = time.time()
	recv_data = bytearray(data)
	recv_data = recv_data[0:-3]
	print("Response from PLC at: ",recv_time)

	return recv_data
