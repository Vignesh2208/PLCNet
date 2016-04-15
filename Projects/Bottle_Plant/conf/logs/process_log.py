import os
import datetime
import math

n_nodes = 3
node = 0
Packet = {}

folder = 'routing_logs'
for the_file in os.listdir(folder):
	file_path = os.path.join(folder, the_file)
	try:
		if os.path.isfile(file_path):
			os.unlink(file_path)
	except Exception as e:
		print(e)


while node <= n_nodes :
	if os.path.isfile("node_" + str(node) + "_log"):
		with open("node_" + str(node) + "_log",'r') as f:
			data = f.read().splitlines(True)
		for line in data :

			ls = line.split(',')

			if len(ls) > 0 :
				dt = datetime.datetime.strptime(ls[0], "%Y-%m-%d %H:%M:%S.%f")
				direction = ls[1]
				hashval = ls[2]
				#print("Dt = ", dt, " direction = ", direction, " hashval = ", hashval)
				if hashval not in Packet.keys() :
					Packet[hashval] = {}
				if direction == "SEND":
					Packet[hashval]["SEND"] = dt
				else:
					Packet[hashval]["RECV"] = dt
				
	node = node + 1

total_elapsed_time = 0
total_elapsed_sq_time = 0
sample_mean = 0.0
sample_variance = 0.0
n_packets = 0

#print(Packet)
for packet_id in Packet.keys():
	if "SEND" in Packet[packet_id].keys() and "RECV" in Packet[packet_id].keys() :
		a = Packet[packet_id]["SEND"]
		b = Packet[packet_id]["RECV"]
		d = b - a
		#print(d.total_seconds())
		elapsed_time = float(d.total_seconds())
		total_elapsed_time = total_elapsed_time + elapsed_time
		total_elapsed_sq_time = total_elapsed_sq_time + (elapsed_time*elapsed_time)
		n_packets = n_packets + 1

if n_packets > 1 :
	sample_mean = float(total_elapsed_time)/n_packets
	sample_variance = (float(total_elapsed_sq_time)/(n_packets - 1)) - (float(n_packets)/(n_packets-1))*(sample_mean*sample_mean)
	print("Mean delay : ", sample_mean)
	print("Std dev: ", math.sqrt(sample_variance))
