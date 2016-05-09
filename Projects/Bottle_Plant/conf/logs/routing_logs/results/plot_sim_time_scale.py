import numpy as np
import matplotlib.pyplot as plt

from statistics import mean,stdev

N_packets = 38
tdf = [2,3,4,5,6]
factor = 1
undilated_throughput = [54.87,54.87,54.87,54.87,54.87]
undilated_latency = [0.1733,0.1733,0.1733,0.1733,0.1733]

data_1_throughput = [32,33,35,35,34]
data_1_throughput[:] = [float(x*100) / float(N_packets) for x in data_1_throughput]
data_1_latency = [4.53722,4.24712,3.8326,3.954,4.254]
data_1_latency[:] = [float(x) / float(5) for x in data_1_latency]
#data_1_n_hops = [13,13.07404,13.0232,13,13.06232]

data_2_throughput = [35,33,32,34]
data_2_throughput[:] = [float(x*100) / float(N_packets) for x in data_2_throughput]

data_2_latency = [3.670,3.955,4.348,4.085]
data_2_latency[:] = [float(x) / float(5) for x in data_2_latency]

#data_2_n_hops = [13,13,13.1851,13,13.10]

data_3_throughput = [35,34,35,36]
data_3_throughput[:] = [float(x*100) / float(N_packets) for x in data_3_throughput]

data_3_latency = [4.363,4.262,4.552,4.5228]
data_3_latency[:] = [float(x) / float(5) for x in data_3_latency]
#data_3_n_hops = [13.031,13.031,13.039,13,13]

#data_4_throughput = [90,95,82,96,91]
data_4_throughput = [32,34,36,36]
data_4_throughput[:] = [float(x*100) / float(N_packets) for x in data_4_throughput]

#data_4_latency = [1.60799,1.1621,1.5403,1.2914,1.31]
data_4_latency = [4.2315,4.476,4.347,4.906]
data_4_latency[:] = [float(x) / float(5) for x in data_4_latency]
#data_4_n_hops = [13,13,13,13.0625,13.0439]

data_5_throughput = [34,38,35,37]
data_5_throughput[:] = [float(x*100) / float(N_packets) for x in data_5_throughput]

#data_4_latency = [1.60799,1.1621,1.5403,1.2914,1.31]
data_5_latency = [4.361,4.388,4.543,4.333]
data_5_latency[:] = [float(x) / float(5) for x in data_5_latency]




data_1_throughput_mean = mean(data_1_throughput)
data_1_throughput_stddev = factor*stdev(data_1_throughput)
data_1_latency_mean = mean(data_1_latency)
data_1_latency_stddev = factor*stdev(data_1_latency)

data_2_throughput_mean = mean(data_2_throughput)
data_2_throughput_stddev = factor*stdev(data_2_throughput)
data_2_latency_mean = mean(data_2_latency)
data_2_latency_stddev = factor*stdev(data_2_latency)

data_3_throughput_mean = mean(data_3_throughput)
data_3_throughput_stddev = factor*stdev(data_3_throughput)
data_3_latency_mean = mean(data_3_latency)
data_3_latency_stddev = factor*stdev(data_3_latency)

data_4_throughput_mean = mean(data_4_throughput)
data_4_throughput_stddev = factor*stdev(data_4_throughput)
data_4_latency_mean = mean(data_4_latency)
data_4_latency_stddev = factor*stdev(data_4_latency)

data_5_throughput_mean = mean(data_5_throughput)
data_5_throughput_stddev = factor*stdev(data_5_throughput)
data_5_latency_mean = mean(data_5_latency)
data_5_latency_stddev = factor*stdev(data_5_latency)

y_throughput = [data_1_throughput_mean,data_2_throughput_mean,data_3_throughput_mean,data_4_throughput_mean,data_5_throughput_mean]
y_latency = [data_1_latency_mean,data_2_latency_mean,data_3_latency_mean,data_4_latency_mean,data_5_latency_mean]

y_throughput_stddev = [data_1_throughput_stddev,data_2_throughput_stddev,data_3_throughput_stddev,data_4_throughput_stddev,data_5_throughput_stddev]
y_latency_stddev = [data_1_latency_stddev,data_2_latency_stddev,data_3_latency_stddev,data_4_latency_stddev,data_5_latency_stddev]
plt.figure()

fig, ax = plt.subplots()

ax.errorbar(tdf,y_throughput,xerr=0, yerr=y_throughput_stddev, linestyle='-.', marker='o', color='b',label="Dialated")

ax.plot(tdf,undilated_throughput,linestyle="--",color='black',label="Undilated")

x1,x2,y1,y2 = plt.axis()
plt.axis((1,7,50,100))
plt.ylabel('Throughput (%)')
plt.xlabel('TDF')
plt.title("Average Throughput vs TDF")

plt.legend(loc="center right", shadow=True, fancybox=True)


#y = np.arange(y_throughput)
#z = y


i = 0
while i < len(tdf) :

	ax.annotate(round(y_throughput[i],3), (tdf[i],y_throughput[i] + 1))
	i = i + 1

ax.annotate(undilated_throughput[0],(1.6,undilated_throughput[0]))


plt.show()


plt.figure()

fig, ax = plt.subplots()
ax.errorbar(tdf,y_latency,xerr=0, yerr=y_latency_stddev, linestyle='-.', marker='o', color='b',label="Global dialation = 5")

ax.plot(tdf,undilated_latency,linestyle="--",color='black',label="Undilated")

x1,x2,y1,y2 = plt.axis()
plt.axis((1,7,0,2))
plt.ylabel('End-to-End Latency (sec)')
plt.xlabel('TDF')
plt.title("Average End-to-End Latency vs TDF")

#plt.legend(loc = "best", shadow=True, fancybox=True)
plt.legend(loc = "best")

i = 0
while i < len(tdf) :
	ax.annotate(round(y_latency[i],3), (tdf[i],y_latency[i]+0.05))
	i = i + 1

ax.annotate(undilated_latency[0],(1.6,undilated_latency[0]))


plt.show()


plt.figure()
N_Cores = [5,7,10,12,15,17,20,22,25]
Variance = [0,0,0.0725,0.07415,0.0932,0.14231,1.6226,2.0748,2.255]

fig, ax = plt.subplots()
ax.errorbar(N_Cores,Variance,xerr=0, yerr=0, linestyle='-.', marker='o', color='b')

x1,x2,y1,y2 = plt.axis()
plt.axis((3,27,0,4))
plt.ylabel('Std dev of Number of Hops')
plt.xlabel('Number of LXCs per CORE ')
plt.title("Variance in Number of Routing Hops with increased load")

#plt.legend(loc = "best", shadow=True, fancybox=True)
plt.legend(loc = "best")

i = 0
while i < len(N_Cores) :
	ax.annotate(round(Variance[i],3), (N_Cores[i],Variance[i]+0.05))
	i = i + 1


plt.show()


