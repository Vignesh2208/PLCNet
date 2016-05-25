import os 
import sys
import time
import datetime
import math
import socket
import select
import random

recv_time_val = 5.0
conn_time = 5.0
Mem_Type 	= "HOLDING_REG"
Access_Type = "READ"
Reg_Addr 	= 2
N_Regs 		= 1
Slave_PLC	= 0
TI 			= 0x10
coil_mem = {}
reg_mem = {}


script_path = os.path.dirname(os.path.realpath(__file__))
script_path_list = script_path.split('/')

root_directory = "/"
for entry in script_path_list :
	
	if entry == "awlsim-0.42" :
		root_directory = root_directory + "awlsim-0.42"
		break
	else :
		if len(entry) > 0 :
			root_directory = root_directory + entry + "/"


modbus_python_binding_path = root_directory + "/awlsim/modbus"
if modbus_python_binding_path not in sys.path :
	sys.path.append(modbus_python_binding_path)


from modbus_msg import *
from modbus_master import *



def hostname_to_ip(hostname) :

	host_id = int(hostname)
	conf_directory = os.path.dirname(os.path.realpath(__file__))
	conf_file = conf_directory + "/PLC_Config/lxc" + str(host_id) + "-0"

	print "Conf file = ", conf_file

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

	return resolved_hostname


TCP_REMOTE_IP = hostname_to_ip(0)
TCP_REMOTE_PORT = 502

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
		if time.time() - s_time > conn_time :
			print("Conn time expired. Quitting")
			sys.exit()
		else:
			time.sleep(0.1)
		client_socket.close()		
	attempt_no = attempt_no + 1

print("Connection established at " + str(datetime.datetime.now()))
BUFFER_SIZE = 4096
master = ModBusMaster(coil_mem,reg_mem)
msg_to_send,error = master.construct_request(Mem_Type,Access_Type,Reg_Addr,N_Regs,TI,Slave_PLC)
client_socket.settimeout(recv_time_val)		
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

print "Waiting for data ..."
try:
	data = client_socket.recv(BUFFER_SIZE)
except socket.timeout:
	print "Recv timeout"
	sys.exit()

recv_time = time.time()
recv_data = bytearray(data)
recv_data = recv_data[0:-3]
print("Response from PLC : ",recv_data)
master.process_response(recv_data)
print "Read Register Value from PLC = ", reg_mem["Holding_Register_2"]
