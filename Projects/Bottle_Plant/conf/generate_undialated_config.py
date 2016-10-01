import os
import time

Max_nodes = 63
node = 0
scriptDir = os.path.dirname(os.path.realpath(__file__))
awlsimDir = scriptDir + "/../../../"
reader_path = os.path.dirname(os.path.realpath(__file__)) + "../../../s3fnet-lxc/lxc-command/reader_modified"

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
			f.write("python " + scriptDir + "/PLC_Config/udp_reader.py\n")
		else:
			f.write(awlsimDir + "/tests/run.sh -e 0 --node " + str(node) + " " + awlsimDir + "/tests/modbus/bottle_plant_node/bottle_plant_node.awl\n")

	node = node + 1

print "Commands sent to all lxcs. Waiting for 10 secs"
time.sleep(20)
print "Killing all lxcs"
node = 0
while node <= Max_nodes :
	lxc_name = "lxc" + str(node) + "-0"
	print "Killing : ", lxc_name
	os.system("sudo lxc-kill -n " + str(lxc_name))
	node = node + 1
