Experiment_Name		=	"Bottle_Plant"
Run_Time			=	10
Dilation_Factor		=	5
Number_Of_Nodes		=	7
ModBus_Network_Type	=	"IP"
Node_to_router_delay   = 1	# in ms
Router_to_router_delay = 2	# in ms
AwlScript_Directory = "/home/vignesh/Desktop/PLCs/awlsim-0.42/tests/modbus/bottle_plant_node"
Exp_config_file = "exp_config"
Topology_config_file = "topology_config"



with open(Exp_config_file,'w') as f :

	f.write("START_OF_EXPERIMENT_CONFIGURATION\n")
	f.write("Experiment_Name	=	" + Experiment_Name + "\n")
	f.write("Run_Time			=	" + str(Run_Time) + "\n")
	f.write("Dilation_Factor	=	" + str(Dilation_Factor) + "\n")
	f.write("Number_Of_Nodes	=	" + str(Number_Of_Nodes) + "\n")
	f.write("ModBus_Network_Type =  " + ModBus_Network_Type + "\n")
	f.write("END_OF_EXPERIMENT_CONFIGURATION\n\n")
	f.write("START_OF_GENERAL_CONFIGURATION\n")
	node = 1
	while node <= Number_Of_Nodes :
		conn_no = 1
		while conn_no < 4 :
			if node == 1 and conn_no == 1 :
				f.write("1,		1,		502,	502,	2,		True\n") 
			elif conn_no == 1:
				f.write(str(node) + ",		1,		502,	502,	" + str(2*node) + ",		True\n")
			elif conn_no == 2 :
				f.write(str(node) + ",		2,		502,	502,	" + str(2*node) + ",		False\n")
			elif conn_no == 3 :
				f.write(str(node) + ",		3,		502,	502,	" + str(2*node + 1) + ",		False\n")


			conn_no = conn_no + 1
		node = node + 1


	f.write("END_OF_GENERAL_CONFIGURATION\n\n")
	f.write("START_OF_AWL_SCRIPT_CONFIGURATION\n")
	node = 1
	while node <= Number_Of_Nodes :
		f.write(str(node) +",		" + AwlScript_Directory + "/bottle_plant_node.awl\n")
		node = node + 1


	f.write("END_OF_AWL_SCRIPT_CONFIGURATION\n\n")
	f.write("START_OF_DATA_AREA_CONFIGURATION\n")
	node = 1
	while node <= Number_Of_Nodes :
		conn_no = 1
		while conn_no < 4 :
			f.write(str(node) + ",		" + str(conn_no) + ",		(3:" + str(conn_no + 3) + ":1:100)\n")
			conn_no = conn_no + 1
		node = node + 1

	f.write("END_OF_DATA_AREA_CONFIGURATION\n\n")

with open(Topology_config_file,'w') as f:
	node = 1
	while node <= Number_Of_Nodes + 1:
		f.write("(" + str(node - 1) + "-" + "R" + str(node) + "),		" + str(Node_to_router_delay) + "\n")
		node = node + 1



	node = 1
	while node <= int(Number_Of_Nodes/2) :
		f.write("(" + "R" + str(node) + "-R" + str(2*node) + "),		" + str(Router_to_router_delay) + "\n")
		f.write("(" + "R" + str(node) + "-R" + str(2*node+1) + "),		" + str(Router_to_router_delay) + "\n")
		node = node + 1

	f.write("(" + "R1" + "-R" + str(Number_Of_Nodes + 1) + "),		" + str(Router_to_router_delay) + "\n")