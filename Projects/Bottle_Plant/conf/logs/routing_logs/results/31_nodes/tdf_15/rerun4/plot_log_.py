import os
import datetime
import math
import sys

n_nodes = 63
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
cdf_bins = {}
n_bins = 25
delay_per_bin = 4
cdf_stddev = {}
stddev_list = []
min_stddev = 0.0
max_stddev = 0.01

j = 0
while j <= n_bins :
	cdf_bins[j] = 0
	cdf_stddev[j] = 0
	j = j + 1

#print(Packet)
total_neg_packets = 0
total_outlierpackets = 0
min_negative_delay = 0
min_packet_send_time = datetime.datetime.now()
total_positive_packets = 0

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
			bin_no = int(m_elapsed_time/delay_per_bin)
			if bin_no > n_bins :
				cdf_bins[n_bins] = cdf_bins[n_bins] + 1
			else :			
				cdf_bins[bin_no]  = cdf_bins[bin_no] + 1

			total_positive_packets = total_positive_packets + 1
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
	
node = 0

while node <= n_nodes :
	if os.path.isfile(experiment_name + "node_" + str(node) + "_cycle_statistics_log"):
		with open(experiment_name + "node_" + str(node) + "_cycle_statistics_log",'r') as f:
			data = f.read().splitlines(True)
		for line in data :

			ls = line.split(':')

			if len(ls) > 0 :
				if ls[0].startswith("Sample variance") :
					variance = float(ls[1])
					stddev = math.sqrt(variance)
					stddev_list.append(stddev)
					break
	node = node + 1

				
delay_per_bin = float(max_stddev - min_stddev)/float(n_bins)
for stddev in stddev_list :
	bin_no = int(stddev/delay_per_bin)
	if bin_no > n_bins :
		cdf_stddev[n_bins] = cdf_stddev[n_bins] + 1
	else :
		cdf_stddev[bin_no] = cdf_stddev[bin_no] + 1

bin_no = 1
while bin_no <= n_bins :
	cdf_bins[bin_no] = cdf_bins[bin_no-1] + cdf_bins[bin_no]
	cdf_stddev[bin_no] = cdf_stddev[bin_no-1] + cdf_stddev[bin_no]
	bin_no = bin_no + 1

bin_no = 0
while bin_no <= n_bins :
	cdf_bins[bin_no] = float(cdf_bins[bin_no])/total_positive_packets
	cdf_stddev[bin_no] = float(cdf_stddev[bin_no])/n_nodes
	bin_no = bin_no + 1

print "CDF delay = ", cdf_bins
print "CDF stddev = ", cdf_stddev

