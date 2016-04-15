import socket
import sys
import time
from datetime import datetime

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port on the server given by the caller
server_address = (sys.argv[1], 10000)
print >>sys.stderr, 'connecting to %s port %s' % server_address
sock.connect(server_address)

try:

	i = 0
	while i < 20 :
	
		message = 'This is the message.  It will be repeated.'
		print >>sys.stderr, 'sending "%s"' % message
		sock.sendall(message)
		#print "Send time : ", str(time.time())
		print "Send time : ", str(datetime.now())
		sys.stdout.flush()

		amount_received = 0
		amount_expected = len(message)
		while amount_received < amount_expected:
			data = sock.recv(amount_expected)
			amount_received += len(data)
			print >>sys.stderr, 'received "%s"' % data

		i = i + 1

finally:
	sock.close()