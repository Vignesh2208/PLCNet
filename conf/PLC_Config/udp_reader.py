'''
	Simple udp socket server
'''
 
import socket
import sys
 
HOST = ''   # Symbolic name meaning all available interfaces
PORT = 8888 # Arbitrary non-privileged port
 
# Datagram (udp) socket
try :
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	print 'Socket created'
except socket.error, msg :
	print 'Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	sys.exit()
 
 
# Bind socket to local host and port
try:
	s.bind((HOST, PORT))
except socket.error , msg:
	print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	sys.exit()
	 
print("Socket bind complete")
sys.stdout.flush()
 
#now keep talking with the client
while 1:
	# receive data from client (data, addr)
	d,addr = s.recvfrom(4096)
	data = d
	print "Received Message : ", d
	#print 'Message[' + addr[0] + ':' + str(addr[1]) + '] - ' + data.strip()
	sys.stdout.flush()
	 
s.close()