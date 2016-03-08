import select
import set_connection
import time
import sys
import os

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



set_connection.set_connection(1,0,0)

server_fd = os.open("/dev/s3fserial0",os.O_RDWR)

READ_ONLY = select.POLLIN 
WRITE_ONLY = select.POLLOUT	


print "Start time = ", time.time()

recv_msg = bytearray()
while True :
	poller = select.poll()
	poller.register(server_fd, READ_ONLY)
	events = poller.poll(5000)
	if len(events) == 0 :
		print "End time = ", time.time()
		print "Serial Recv TIMEOUT Done !!!!!!!!!!!!!!!!"
		sys.exit(0)

	resume_poll = True
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
			
	print "Recv msg = ",recv_msg
	recv_data = process_incoming_frame(recv_msg)
	print "Recv data = ",recv_data
	break

		