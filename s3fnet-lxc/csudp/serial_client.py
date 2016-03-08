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

msg_to_send = ""
i = 0
while i < 250 :
	msg_to_send = msg_to_send + "H"
	i = i + 1


set_connection.set_connection(0,1,0)

client_fd = os.open("/dev/s3fserial0",os.O_RDWR)

READ_ONLY = select.POLLIN 
WRITE_ONLY = select.POLLOUT	

send_msg = frame_outgoing_message(msg_to_send)
print "Start time = ", time.time()
print "msg to send = ", send_msg
n_wrote = 0
while n_wrote < len(send_msg) :
	poller = select.poll()
	poller.register(client_fd, WRITE_ONLY)
	events = poller.poll(5000)
	if len(events) == 0 :
		print "End time = ", time.time()
		print "Serial Write TIMEOUT Done !!!!!!!!!!!!!!!!"
		sys.exit(0)

	n_wrote = n_wrote + os.write(client_fd,send_msg[n_wrote:])

print "data sent"