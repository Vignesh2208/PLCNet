"""
Bar chart demo with pairs of bars grouped for easy comparison.
"""
import numpy as np
import matplotlib.pyplot as plt

from statistics import mean,stdev


n_groups = 4

means_t1 = (0.80803,0.67358,0.67333,0.5180)
means_t2 = (0.1616677,0.22518,0.23111,0.26509)
means_t3 = (0.02693,0.07629,0.07555,0.13372)
means_t4 = (0.003366,0.0249,0.02,0.0823)



fig, ax = plt.subplots()

index = np.arange(n_groups)
bar_width = 0.1

opacity = 0.7
error_config = {'ecolor': '0.3'}

rects1 = plt.bar(index, means_t1, bar_width,
                 alpha=opacity,
                 color='b',
                 yerr=0,
                 error_kw=error_config,
                 label='Thread 1')

rects2 = plt.bar(index + bar_width, means_t2, bar_width,
                 alpha=opacity,
                 color='r',
                 yerr=0,
                 error_kw=error_config,
                 label='Thread 2')

rects1 = plt.bar(index + 2*bar_width, means_t3, bar_width,
                 alpha=opacity,
                 color='g',
                 yerr=0,
                 error_kw=error_config,
                 label='Thread 3')

rects2 = plt.bar(index + 3*bar_width, means_t4, bar_width,
                 alpha=opacity,
                 color='y',
                 yerr=0,
                 error_kw=error_config,
                 label='Thread 4')

plt.xlabel('Cycle duration (t)')
plt.ylabel('Thread Share per cycle')
plt.title('Thread execution share')
plt.xticks(index + 2*bar_width, ('t = 25', 't = 50', 't = 75', 't = 100'))
plt.legend()

plt.tight_layout()
plt.show()


tdf = [5,7,9,11,13,15]

N_packets = 100

undilated_throughput = [54.87,54.87,54.87,54.87,54.87,54.87]
undilated_latency = [0.1733,0.1733,0.1733,0.1733,0.1733,0.1733]
factor = 0.8765 
#factor = 1
data_1_throughput = [84,83,84,86,81]
data_1_latency = [1.426033,1.4181,1.378580,1.3809,1.42035]
#data_1_n_hops = [13,13.07404,13.0232,13,13.06232]

data_2_throughput = [86,85,81,85,87]
data_2_latency = [1.33618,1.3167,1.31784,1.30,1.2872]
#data_2_n_hops = [13,13,13.1851,13,13.10]

data_3_throughput = [95,95,93,91,95]
data_3_latency = [1.234,1.30,1.245,1.21,1.59]
#data_3_n_hops = [13.031,13.031,13.039,13,13]

#data_4_throughput = [90,95,82,96,91]
data_4_throughput = [90,95,96,91]
#data_4_latency = [1.60799,1.1621,1.5403,1.2914,1.31]
data_4_latency = [1.60799,1.1621,1.2914,1.31]
#data_4_n_hops = [13,13,13,13.0625,13.0439]

data_5_throughput = [91,91,94,93,95]
data_5_latency = [1.4861,1.544302,1.569,1.2832,1.383]
#data_5_n_hops = 

data_6_throughput = [93,92,93,92]
data_6_latency = [1.206,1.2336,1.17763,1.26134]
#data_6_n_hops = 

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

data_6_throughput_mean = mean(data_6_throughput)
data_6_throughput_stddev = factor*stdev(data_6_throughput)
data_6_latency_mean = mean(data_6_latency)
data_6_latency_stddev = factor*stdev(data_6_latency)

y_throughput = [data_1_throughput_mean,data_2_throughput_mean,data_3_throughput_mean,data_4_throughput_mean,data_5_throughput_mean,data_6_throughput_mean]
y_latency = [data_1_latency_mean,data_2_latency_mean,data_3_latency_mean,data_4_latency_mean,data_5_latency_mean,data_6_latency_mean]

y_throughput_stddev = [data_1_throughput_stddev,data_2_throughput_stddev,data_3_throughput_stddev,data_4_throughput_stddev,data_5_throughput_stddev,data_6_throughput_stddev]
y_latency_stddev = [data_1_latency_stddev,data_2_latency_stddev,data_3_latency_stddev,data_4_latency_stddev,data_5_latency_stddev,data_6_latency_stddev]
plt.figure()

fig, ax = plt.subplots()

ax.errorbar(tdf,y_throughput,xerr=0, yerr=y_throughput_stddev, linestyle='-.', marker='o', color='b',label="Global dialation = 1")

ax.plot(tdf,undilated_throughput,linestyle="--",color='black',label="Undilated")

x1,x2,y1,y2 = plt.axis()
plt.axis((4,16,50,100))
plt.ylabel('Throughput (%)')
plt.xlabel('TDF')
plt.title("Average Throughput vs TDF")

plt.legend(loc="best", shadow=True, fancybox=True)


#y = np.arange(y_throughput)
#z = y


i = 0
while i < len(tdf) :

	ax.annotate(y_throughput[i], (tdf[i],y_throughput[i] + 1))
	i = i + 1

ax.annotate(undilated_throughput[0],(4.2,undilated_throughput[0]))


#plt.show()


plt.figure()

fig, ax = plt.subplots()
ax.errorbar(tdf,y_latency,xerr=0, yerr=y_latency_stddev, linestyle='-.', marker='o', color='b',label="Global dialation = 1")

ax.plot(tdf,undilated_latency,linestyle="--",color='black',label="Undilated")

x1,x2,y1,y2 = plt.axis()
plt.axis((4,16,0,2))
plt.ylabel('End-to-End Latency (sec)')
plt.xlabel('TDF')
plt.title("Average End-to-End Latency vs TDF")

#plt.legend(loc = "best", shadow=True, fancybox=True)
plt.legend(loc = "best")

i = 0
while i < len(tdf) :
	ax.annotate(round(y_latency[i],3), (tdf[i],y_latency[i]+0.05))
	i = i + 1

ax.annotate(undilated_latency[0],(4.2,undilated_latency[0]))


#plt.show()

Percentage_Lost_without_TDFs = [0.0, 0.0, 0.0, 0.5025, 0.0, 0.5025, 0.5025, 2.0100, 2.713, 24.321, 46.130, 61.708, 68.743, 77.386]
N_Cores = [1,3,5,7,9,11,13,15,17,19,20,21,23,25]
Std_Dev_Percentage_Lost_without_TDFs = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0347366975866341, 14.18332250236633, 12.91525720182051, 8.612778004272798, 3.5304860442712287, 1.2308993682336573]

plt.figure()

fig, ax = plt.subplots()
ax.errorbar(N_Cores,Percentage_Lost_without_TDFs,xerr=0, yerr=Std_Dev_Percentage_Lost_without_TDFs, linestyle='-.', marker='o', color='b')

ax.plot(tdf,undilated_latency,linestyle="--",color='black',label="Undilated")

x1,x2,y1,y2 = plt.axis()
plt.axis((0,28,0,100))
plt.ylabel('Percentage of Lost Packets')
plt.xlabel('Number of LXCs per Core')
plt.title("Variation of Througput with increased Load")

i = 0
while i < len(Percentage_Lost_without_TDFs) :
	ax.annotate(round(Percentage_Lost_without_TDFs[i],2), (N_Cores[i]+0.25,Percentage_Lost_without_TDFs[i] + 1))
	i = i + 1

plt.show()
