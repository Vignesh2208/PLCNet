import definitions
from definitions import conf_directory

import os

conf_directory = conf_directory + "/PLC_Config"
hosts_file  = conf_directory + "/hosts"

with open(hosts_file,"w") as f:
	pass

i = 0
while(1) :
	if os.path.isfile(conf_directory + "/" + str(i)) :
		conf_file = conf_directory + "/" + str(i)
		lines = [line.rstrip('\n') for line in open(conf_file)]			
		for line in lines :
			line = ' '.join(line.split())
			line = line.split('=')
			if len(line) > 1 :
				#print(line)
				parameter = line[0]
				value= line[1]
				if "lxc.network.ipv4" in parameter :
					value = value.replace(' ',"")
					value = value.split("/")
					print("Dest IP : ", value[0])
					resolved_hostname = value[0]
					break


		print("Resolved hostname = ",resolved_hostname)
		with open(hosts_file,"a") as f :
			f.write(str(i) + ":" + resolved_hostname + "\n")
	else :
		with open(hosts_file,"a") as f :
			f.write("IDS:" + resolved_hostname + "\n")
		break
	i = i + 1
