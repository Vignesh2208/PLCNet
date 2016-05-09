import os
import datetime
import math
import sys
import numpy as np
import matplotlib.pyplot as plt

from statistics import mean,stdev




node_sizes = [7,15,31,63]
#node_sizes = [31,63]
n_bins = 10
cycle_stddev = {}
cycle_stddev_prob = {}
min_stddev = 0.0
max_stddev = 0.01
min_stddev_undilated = 0.05
max_stddev_undilated = 0.15
#max_stddev = 0.01

experiment_name = ""
Topology_data = {}

dilation_type = ["undialated","tdf_15"]
#dilation_type = ["tdf_15"]

def find_mean(ls) :
	sample_sum = 0.0
	for entry in ls :
		sample_sum = sample_sum + entry
	return float(sample_sum)/float(len(ls))

def find_sample_stddev(ls):
	mean = find_mean(ls)
	sample_sq_sum = 0.0
	n = len(ls)
	for entry in ls:
		sample_sq_sum = sample_sq_sum + (entry*entry)

	sample_variance = float(sample_sq_sum)/(n-1) - (float(n)/float(n-1))*(mean*mean)
	return math.sqrt(sample_variance)


for dilation in dilation_type :
	Topology_data[dilation] = {}
	for curr_node_size in node_sizes :

		delay_per_bin = 4		
		Topology_data[dilation][curr_node_size] = {}
		Topology_data[dilation][curr_node_size]["cdf_packet_delay"] = {}
		Topology_data[dilation][curr_node_size]["cdf_stddev"] = {}
		stddev_list = []

		n_nodes = curr_node_size
		Packet = {}	
		run_no = 1
		while 1 :
			if dilation == "undialated":
				experiment_name = "/home/vignesh/Desktop/PLCs/awlsim-0.42/Projects/Bottle_Plant/conf/logs/routing_logs/results/" + str(n_nodes) + "_nodes/"  + dilation + "/run" + str(run_no)
			else :
				experiment_name = "/home/vignesh/Desktop/PLCs/awlsim-0.42/Projects/Bottle_Plant/conf/logs/routing_logs/results/" + str(n_nodes) + "_nodes/"  + dilation + "/rerun" + str(run_no)
			if not os.path.isdir(experiment_name) :
				break
			node = 0
			Packet[run_no] = {}
			while node <= n_nodes :
				if os.path.isfile(experiment_name + "/node_" + str(node) + "_log"):
					with open(experiment_name + "/node_" + str(node) + "_log",'r') as f:
						data = f.read().splitlines(True)
					for line in data :
						ls = line.split(',')
						if len(ls) > 0 :
							dt = datetime.datetime.strptime(ls[0], "%Y-%m-%d %H:%M:%S.%f")
							direction = ls[1]
							hashval = ls[2]
							if hashval not in Packet[run_no].keys() :
								Packet[run_no][hashval] = {}
							if direction == "SEND":
								if "SEND" in Packet[run_no][hashval].keys():
									print("Duplicate @@@@@@")
								Packet[run_no][hashval]["SEND"] = dt
							else:
								Packet[run_no][hashval]["RECV"] = dt

				if os.path.isfile(experiment_name + "/node_" + str(node) + "_cycle_statistics_log") and node != n_nodes :
					with open(experiment_name + "/node_" + str(node) + "_cycle_statistics_log",'r') as f:
						data = f.read().splitlines(True)
					for line in data :

						ls = line.split(':')
						if len(ls) > 0 :
							if ls[0].startswith("Sample variance") :
								variance = float(ls[1])
								stddev = math.sqrt(variance)
								if run_no == 1 :									

									stddev_list.append(stddev)
								break
					
				node = node + 1
			run_no = run_no + 1


		if dilation == "undialated" and curr_node_size == 63 :	
			print "Stddev =", stddev_list
		j = 0
		while j <= n_bins :
			Topology_data[dilation][curr_node_size]["cdf_packet_delay"][j] = 0
			Topology_data[dilation][curr_node_size]["cdf_stddev"][j] = 0
			
			j = j + 1

		#print(Packet)
		total_elapsed_time = 0
		total_elapsed_sq_time = 0
		sample_mean = 0.0
		sample_variance = 0.0
		n_packets = 0
		total_neg_packets = 0
		total_outlierpackets = 0
		min_negative_delay = 0
		min_packet_send_time = datetime.datetime.now()
		total_positive_packets = 0
		curr_run_no = 1
		print Packet.keys()
		n_packets_run = []
		count = 0
		
		while 1 :
			n_pkts_curr_run = 0
			
			if not curr_run_no in Packet.keys():
				break
			for packet_id in Packet[curr_run_no].keys():
				if "SEND" in Packet[curr_run_no][packet_id].keys() and "RECV" in Packet[curr_run_no][packet_id].keys() :
					a = Packet[curr_run_no][packet_id]["SEND"]
					b = Packet[curr_run_no][packet_id]["RECV"]
					d = b - a					
					elapsed_time = float(d.total_seconds())
					if elapsed_time > 0 :
						m_elapsed_time = int(elapsed_time*1000)
						bin_no = int(m_elapsed_time/delay_per_bin)
						if bin_no > n_bins :
							
							Topology_data[dilation][curr_node_size]["cdf_packet_delay"][n_bins] = Topology_data[dilation][curr_node_size]["cdf_packet_delay"][n_bins] + 1
						else :	
							
							Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no]  = Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no] + 1

						total_positive_packets = total_positive_packets + 1
					else:
						total_neg_packets = total_neg_packets + 1					

					if elapsed_time < 0.5 and elapsed_time > -0.5 :

						total_elapsed_time = total_elapsed_time + elapsed_time
						total_elapsed_sq_time = total_elapsed_sq_time + (elapsed_time*elapsed_time)
						n_packets = n_packets + 1
						n_pkts_curr_run = n_pkts_curr_run + 1
					else:
						total_outlierpackets = total_outlierpackets + 1

			n_packets_run.append(n_pkts_curr_run/3)
			curr_run_no = curr_run_no + 1
		print  curr_node_size, n_packets
		
		if n_packets > 1 :
			sample_mean = float(total_elapsed_time)/n_packets
			sample_variance = (float(total_elapsed_sq_time)/(n_packets - 1)) - (float(n_packets)/(n_packets-1))*(sample_mean*sample_mean)
			print("Mean delay : ", sample_mean)
			print("Std dev: ", math.sqrt(sample_variance))
			print("n_packets : ", n_packets)
			print("total_positive_packets : ", total_positive_packets)
			
		if dilation == "tdf_15"	:
			delay_per_bin = float(max_stddev - min_stddev)/float(n_bins)
		else :
			delay_per_bin = float(max_stddev_undilated - min_stddev_undilated)/float(n_bins)

		for stddev in stddev_list :
			if dilation == "tdf_15":
				bin_no = int(stddev/delay_per_bin)
			else :
				if curr_node_size == 63 :
					bin_no = int((stddev - min_stddev_undilated)/delay_per_bin)
				else :
					bin_no = int(stddev/delay_per_bin)
			if bin_no > n_bins :
				Topology_data[dilation][curr_node_size]["cdf_stddev"][n_bins] = Topology_data[dilation][curr_node_size]["cdf_stddev"][n_bins] + 1
			else :
				Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no] = Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no] + 1

		bin_no = 1
		while bin_no <= n_bins :
			Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no] = Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no-1] + Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no]
			#Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no] = Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no-1] + Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no]
			bin_no = bin_no + 1

		bin_no = 0
		ninety_percentile = 0.0
		while bin_no <= n_bins :
			Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no] = float(Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no])/total_positive_packets
			if Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no] >= 0.9 and Topology_data[dilation][curr_node_size]["cdf_packet_delay"][bin_no-1] <= 0.9 :
				ninety_percentile = bin_no*0.004
			#Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no] = float(Topology_data[dilation][curr_node_size]["cdf_stddev"][bin_no])/(n_nodes* (run_no-1))
			bin_no = bin_no + 1

		print "Node = ", curr_node_size," CDF delay = ", Topology_data[dilation][n_nodes]["cdf_stddev"]		#print "Node = ", curr_node_size," CDF stddev = ", cdf_stddev
		
		Topology_data[dilation][n_nodes]["mean_delay"] = sample_mean
		Topology_data[dilation][n_nodes]["std_dev"] = math.sqrt(sample_variance)
		Topology_data[dilation][n_nodes]["n_packets"] = n_packets
		#Topology_data[dilation][n_nodes]["cdf_packet_delay"] = cdf_packet_delay
		#Topology_data[dilation][n_nodes]["cdf_stddev"] = cdf_stddev
		if n_nodes == 63 and dilation == "tdf_15" :
			n_packets_run.append(2859/3)	# two values from lost log files
			n_packets_run.append(2751/3)
		Topology_data[dilation][n_nodes]["throughput_mean"] = find_mean(n_packets_run)
		Topology_data[dilation][n_nodes]["throughput_stddev"] = find_sample_stddev(n_packets_run)
		Topology_data[dilation][n_nodes]["90"] = ninety_percentile

		print "N_nodes = ", n_nodes, "Dilation = ", dilation, " 90 percentile delay = ", ninety_percentile


	#plt.figure()
	fig, ax = plt.subplots()
	params = {'legend.fontsize': 23,'legend.linewidth': 4}
	plt.rcParams.update(params)

	pkt_delay = {}
	pkt_delay_prob = {}
	
	cycle_stddev[dilation] = {}
	cycle_stddev_prob[dilation] = {}

	pkt_delay_per_bin = 0.004
	if dilation == "tdf_15":
		cycle_stddev_per_bin = (max_stddev - min_stddev)/n_bins
	else :
		cycle_stddev_per_bin = (max_stddev_undilated - min_stddev_undilated)/n_bins

	for node in node_sizes :
		pkt_delay[node] = []
		pkt_delay_prob[node] = []
		cycle_stddev[dilation][node] = []
		cycle_stddev_prob[dilation][node] = []

		bin_no = 0
		while bin_no <= n_bins :
			if dilation == "tdf_15":
				cycle_stddev[dilation][node].append(cycle_stddev_per_bin*bin_no*1000)				
			else :
				cycle_stddev[dilation][node].append(cycle_stddev_per_bin*bin_no*1000 + min_stddev_undilated*1000)
			#print "Node = ", node
			#print Topology_data[dilation][curr_node_size]
			pkt_delay_prob[node].append(Topology_data[dilation][node]["cdf_packet_delay"][bin_no])
			pkt_delay[node].append(pkt_delay_per_bin*bin_no*1000)
			cycle_stddev_prob[dilation][node].append(Topology_data[dilation][node]["cdf_stddev"][bin_no])
			bin_no = bin_no + 1

		

		#ax.errorbar(pkt_delay[node],pkt_delay_prob[node],xerr=0, yerr=0, linestyle='-.', marker='o', color='b',label=str(node) + " nodes")
		#ax.errorbar(pkt_delay[node],pkt_delay_prob[node],xerr=0, yerr=0, linestyle='-', marker='o', color='b',label=str(node) + " nodes")
		ax.xaxis.set_tick_params(labelsize=25)
		ax.yaxis.set_tick_params(labelsize=25)

		if node == 7 :		
			plt.plot(pkt_delay[node],pkt_delay_prob[node],linewidth=7,linestyle="-",color='red',label=str(node) + " nodes")
		if node == 15 :
			plt.plot(pkt_delay[node],pkt_delay_prob[node],linewidth=7,linestyle="-.",color='blue',label=str(node) + " nodes")
		if node == 31 :
			plt.plot(pkt_delay[node],pkt_delay_prob[node],linewidth=7,linestyle="--",color='green',label=str(node) + " nodes")
		if node == 63 :
			plt.plot(pkt_delay[node],pkt_delay_prob[node],linewidth=7,linestyle=":",color='black',label=str(node) + " nodes")

	x1,x2,y1,y2 = plt.axis()
	plt.ylabel('Probability',fontsize=25)
	plt.xlabel('Packet Delay (ms)',fontsize=25)
	if dilation == "undialated" :
		plt.title("CDF of per Link Packet Delay - Best-Effort Experiment",fontsize=25)
	else :
		plt.title("CDF of per Link Packet Delay - TimeKeeper Experiment",fontsize=25)
	#plt.legend(loc="center right", shadow=True, fancybox=True)
	plt.legend(loc="upper left", shadow=True, fancybox=True)
	#plt.show()


##plt.figure()
#fig, ax = plt.subplots()

##for dilation in dilation_type:
#plt.plot(cycle_stddev["undialated"][63],cycle_stddev_prob["undialated"][63],linestyle="-",color='red',label="undialated")
#plt.plot(cycle_stddev["tdf_15"][63],cycle_stddev_prob["tdf_15"][63],linestyle="-.",color='black',label="dialated")

#x1,x2,y1,y2 = plt.axis()
#plt.ylabel('Probability')
#plt.xlabel('Standard Deviation of Cycle Time (s)')
#plt.title("Comparison of CDF of Standard Deviation of Cycle Times")
#plt.legend(loc="center right", shadow=True, fancybox=True)
#plt.show()

x = node_sizes
throughput_dilated = []
throughput_dilated_stddev = []
throughput_undilated = []
throughput_undilated_stddev = []

N = 5
for node in node_sizes :
	throughput_dilated.append(Topology_data["tdf_15"][node]["throughput_mean"])
	throughput_dilated_stddev.append(Topology_data["tdf_15"][node]["throughput_stddev"])
	throughput_undilated.append(Topology_data["undialated"][node]["throughput_mean"])
	throughput_undilated_stddev.append(Topology_data["undialated"][node]["throughput_stddev"])

#plt.figure()
fig, ax = plt.subplots()
ax.xaxis.set_tick_params(labelsize=25)
ax.yaxis.set_tick_params(labelsize=25)
ax.errorbar(x,throughput_dilated,xerr=0, yerr=throughput_dilated_stddev,linewidth=5, linestyle='-.', marker='^', color='green',label="with TimeKeeper")
ax.errorbar(x,throughput_undilated,xerr=0, yerr=throughput_undilated_stddev,linewidth=5, linestyle='-', marker='^', color='black',label="Best-Effort")

x1,x2,y1,y2 = plt.axis()
plt.ylabel('Througput (Packets/sec)',fontsize=25)
plt.xlabel('Number of Nodes',fontsize=25)
plt.title("Maximum Throughput - TimeKeeper and Best-Effort experiments",fontsize=25)
plt.legend(loc="center right", shadow=True, fancybox=True)

#plt.show()


fig, ax = plt.subplots()
N = len(cycle_stddev_prob["undialated"][63])
menMeans = cycle_stddev_prob["undialated"][63]


## necessary variables
ind = np.arange(N)                # the x locations for the groups
width = 0.35                      # the width of the bars
yTickMarks = [0,10,20,30,40,50,60,70]
## the bars
rects1 = ax.bar(ind, menMeans, width,
                color='black',
                yerr=0,
                error_kw=dict(elinewidth=2,ecolor='black'))

#rects2 = ax.bar(ind+width, womenMeans, width,
#                   color='red',
#                    yerr=0,
#                    error_kw=dict(elinewidth=2,ecolor='black'), hatch="//")

# axes and labels
ax.set_xlim(-width,len(ind)+width)
ax.set_ylim(0,70)
ax.set_ylabel('Number of nodes',fontsize=25)
ax.set_xlabel('Standard deviation of cycle times (ms)',fontsize=25)
ax.set_title('Histogram of Cycle Time Stddev - 63 node Best-Effort Experiment',fontsize=25)
#xTickMarks = ['Group'+str(i) for i in range(1,n_bins)]
#marks = [int(x) for x in cycle_stddev["undialated"][63]]
marks = [50,60,70,80,90,100,110,120,130,140,150]
xTickMarks = marks
ax.set_xticks(ind+width)
xtickNames = ax.set_xticklabels(xTickMarks)
ytickNames = ax.set_yticklabels(yTickMarks)
plt.setp(xtickNames, rotation=0, fontsize=25)
plt.setp(ytickNames, rotation=0, fontsize=25)

## add a legend
#ax.legend( (rects1[0], rects2[0]), ('Undilated', 'Dilated') )

plt.show()



fig, ax = plt.subplots()
N = len(cycle_stddev_prob["tdf_15"][63])
menMeans = cycle_stddev_prob["tdf_15"][63]
#womenStd =   [3, 5, 2, 3, 3]

## necessary variables
ind = np.arange(N)                # the x locations for the groups
width = 0.35                      # the width of the bars

## the bars
rects1 = ax.bar(ind, menMeans, width,
                color='red',
                yerr=0,
                error_kw=dict(elinewidth=2,ecolor='red'),hatch = "//")

# axes and labels
ax.set_xlim(-width,len(ind)+width)
ax.set_ylim(0,70)
ax.set_ylabel('Number of nodes',fontsize=25)
ax.set_xlabel('Standard deviation of cycle times (ms)',fontsize=25)
ax.set_title('Histogram of Cycle Time Stddev - 63 node TimeKeeper Experiment',fontsize=25)
#xTickMarks = ['Group'+str(i) for i in range(1,n_bins)]
marks = [int(x) for x in cycle_stddev["tdf_15"][63]]
xTickMarks = marks
ax.set_xticks(ind+width)


xtickNames = ax.set_xticklabels(xTickMarks)
ytickNames = ax.set_yticklabels(yTickMarks)
plt.setp(xtickNames, rotation=0, fontsize=25)
plt.setp(ytickNames, rotation=0, fontsize=25)

## add a legend
#ax.legend( (rects1[0], rects2[0]), ('Undilated', 'Dilated') )

plt.show()