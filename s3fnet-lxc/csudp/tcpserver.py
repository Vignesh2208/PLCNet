import socket
import sys
import time
from datetime import datetime

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Bind the socket to the address given on the command line
server_name = ''
server_address = (server_name, 10000)
print >>sys.stderr, 'starting up on %s port %s' % server_address
sock.bind(server_address)
sock.listen(1)
print >>sys.stderr, 'waiting for a connection'
connection, client_address = sock.accept()
try:
	print >>sys.stderr, 'client connected:', client_address
	while True:
		data = connection.recv(200)
		print >>sys.stderr, 'received "%s"' % data
		print "Recv time : ", str(datetime.now())
		sys.stdout.flush()
		if data:
			connection.sendall(data)
		else:
			break
finally:
	connection.close()