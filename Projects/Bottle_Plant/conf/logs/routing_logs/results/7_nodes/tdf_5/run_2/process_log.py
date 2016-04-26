import os
import datetime
import math
import sys

n_nodes = 7
node = 0
Packet = {}

arglist = sys.argv

folder = 'routing_logs'
if len(arglist) > 1 :
	experiment_name = folder + "/" + str(arglist[1]) + "/"
else:
	experiment_name = ""

if os.path.isdir(folder):
	for the_file in os.listdir(folder):
		file_path = os.path.join(folder, the_file)
		try:
			if os.path.isfile(file_path):
				os.unlink(file_path)
		except Exception as e:
			print(e)


while node <= n_nodes :
	if os.path.isfile(experiment_name + "node_" + str(node) + "_log"):
		with open(experiment_name + "node_" + str(node) + "_log",'r') as f:
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
					if "SEND" in Packet[hashval].keys():
						print("Duplicate @@@@@@")
					Packet[hashval]["SEND"] = dt
				else:
					Packet[hashval]["RECV"] = dt
				
	node = node + 1

total_elapsed_time = 0
total_elapsed_sq_time = 0
sample_mean = 0.0
sample_variance = 0.0
n_packets = 0
bins = {}
j = 0
while j <= 10 :
	bins[j] = 0
	j = j + 1

#print(Packet)
total_neg_packets = 0
total_outlierpackets = 0
min_negative_delay = 0
min_packet_send_time = datetime.datetime.now()
for packet_id in Packet.keys():
	if "SEND" in Packet[packet_id].keys() and "RECV" in Packet[packet_id].keys() :
		a = Packet[packet_id]["SEND"]
		b = Packet[packet_id]["RECV"]
		d = b - a
		if a < min_packet_send_time :
			min_packet_send_time = a
		#print(d.total_seconds())
		elapsed_time = float(d.total_seconds())
		if elapsed_time > 0 :
			m_elapsed_time = int(elapsed_time*1000)
			bin_no = int(m_elapsed_time / 10)
			if bin_no > 10 :
				print("Elapsed time = ", elapsed_time, " send time = ", a)
				bins[10] = bins[10] + 1
			else :
			
				bins[bin_no]  = bins[bin_no] + 1
		else:
			total_neg_packets = total_neg_packets + 1
			if elapsed_time < min_negative_delay :
				min_negative_delay = elapsed_time

		if elapsed_time < 0.5 and elapsed_time > -0.5 :

			total_elapsed_time = total_elapsed_time + elapsed_time
			total_elapsed_sq_time = total_elapsed_sq_time + (elapsed_time*elapsed_time)
			n_packets = n_packets + 1
		else:
			total_outlierpackets = total_outlierpackets + 1

if n_packets > 1 :
	sample_mean = float(total_elapsed_time)/n_packets
	sample_variance = (float(total_elapsed_sq_time)/(n_packets - 1)) - (float(n_packets)/(n_packets-1))*(sample_mean*sample_mean)
	print("Mean delay : ", sample_mean)
	print("Std dev: ", math.sqrt(sample_variance))
	print("n_packets : ", n_packets)
	print("neg_packets : ", total_neg_packets)
	print("total_outlierpackets : ", total_outlierpackets)
	print("Min negative delay : ", min_negative_delay)
	print("Min packet send time : ", min_packet_send_time)
	
	process_log_output_file = experiment_name + "process_log_out.txt"
	with open(process_log_output_file,"w") as f:
		f.write("Mean delay : " +  str(sample_mean) + "\n")
		f.write("Std dev    : " + str(math.sqrt(sample_variance)) + "\n")
		j = 0
		total_packets = 0
		while j <= 10 :
			print("Bin[",j,"] = ", bins[j])
			f.write("Bin[" + str(j) + "] = " + str(bins[j]) + "\n")
			total_packets = total_packets = bins[j]
			j = j + 1
		f.write("Total packets  : " + str(total_packets) + "\n")
		f.write("Total neg packets : " + str(total_neg_packets) + "\n")
		f.write("total_outlierpackets : " + str(total_outlierpackets) + "\n")
