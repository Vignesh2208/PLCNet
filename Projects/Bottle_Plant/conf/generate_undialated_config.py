import os
import time

Max_nodes = 2
node = 0
reader_path = "/home/vignesh/Desktop/PLCs/awlsim-0.42/s3fnet-lxc/lxc-command/reader_modified"

while node <= Max_nodes :
	lxc_name = "lxc" + str(node) + "-0"
	print "Destroying : ", lxc_name
	#os.system("sudo lxc-kill -n " + str(lxc_name))
	os.system("sudo lxc-destroy -n " + str(lxc_name))
	lxc_ip = "10.10.0." + str(node + 1)
	with open("PLC_Config/" + str(lxc_name),"w") as f :
		f.write("lxc.utsname	=	" + str(lxc_name) + "\n")
		f.write("lxc.network.type	=	veth\n")
		f.write("lxc.network.link 	=	lxcbr0\n")
		f.write("lxc.network.flags	=	up\n")
		f.write("lxc.network.ipv4 	= 	" + str(lxc_ip) + "\n")
		f.write("lxc.cgroup.cpu.shares = 1\n")
		f.write("lxc.cgroup.cpuset.cpus = "+ str(node % 2) + "\n")
	print "Creating : ", lxc_name
	os.system("sudo lxc-create -n " + str(lxc_name) + " -f " + "PLC_Config/" + str(lxc_name))

	node = node + 1
print "All lxcs created"

node = 0
while node <= Max_nodes :
	lxc_name = "lxc" + str(node) + "-0"
	print "Starting lxc ", lxc_name
	os.system("sudo lxc-start -n " + str(lxc_name) + " -d " + reader_path)
	node = node + 1

print "All lxcs started. Waiting 10 secs"
time.sleep(10)
node = 0
while node <= Max_nodes :
	lxc_name = "lxc" + str(node) + "-0"
	with open("/tmp/" + lxc_name,"w") as f :
		if node == Max_nodes :
			f.write("python /home/vignesh/Desktop/PLCs/awlsim-0.42/Projects/Bottle_Plant/conf/PLC_Config/udp_reader.py\n")
		else:
			f.write("/home/vignesh/Desktop/PLCs/awlsim-0.42/tests/run.sh -e 0 --node " + str(node) + " /home/vignesh/Desktop/PLCs/awlsim-0.42/tests/modbus/bottle_plant_node/bottle_plant_node.awl\n")

	node = node + 1

print "Commands sent to all lxcs. Waiting for 20 secs"
time.sleep(20)
print "Killing all lxcs"
node = 0
while node <= Max_nodes :
	lxc_name = "lxc" + str(node) + "-0"
	print "Killing : ", lxc_name
	os.system("sudo lxc-kill -n " + str(lxc_name))
	node = node + 1