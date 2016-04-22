import math

def parse_modbus_ip_topology(conf_directory,curr_conf_file,test_file,topology_file,exp_run_time,exp_name,exp_tdf,exp_n_nodes,Node,N_CPUS):
	Lxcs = {}
	Routers = {}
	lines = [line.rstrip('\n') for line in open(topology_file)]

	for line in lines :
		line = ' '.join(line.split())
		if not line.startswith("#") and len(line) > 1:
			line = line.replace(" ","")
			line = line.split(',')
			if len(line) != 2 :
				ERROR("Error in topology description")
			link = line[0]
			delay = int(line[1])
			link_ls = link[1:-1].split('-')
			if len(link_ls) != 2 :
				ERROR("Error in link description")

			node_1 = link_ls[0]
			node_2 = link_ls[1]

			lxc = None
			router_1 = None
			router_2 = None

			if not node_1.startswith("R") and not node_2.startswith("R") :
				ERROR("Error in link description : atleast one node in link should be router")

			if node_1.startswith("R") and not node_2.startswith("R") :
				router = node_1
				lxc = node_2
			if node_2.startswith("R") and not node_1.startswith("R") :
				router = node_2
				lxc = node_1
			if node_1.startswith("R") and node_2.startswith("R") :
				router_1 = node_1
				router_2 = node_2

			if lxc != None :

				lxc_id = int(lxc)

				if router.endswith("C") :
					router_id = int(router[1:-1])
					isCompromised = 1
				else :
					router_id = int(router[1:])
					isCompromised = 0


				if lxc_id in Lxcs.keys() :
					ERROR("Each node can have only one link attached to a router")


				Lxcs[lxc_id] = {}
				Lxcs[lxc_id]["router"] = router_id
				Lxcs[lxc_id]["delay"] = delay

				if not router_id in Routers.keys() :
					Routers[router_id] = {}
					Routers[router_id]["lxcs"] = []
					Routers[router_id]["routers"] = []
					Routers[router_id]["n_intf"] = 0
					Routers[router_id]["nxt_intf"] = 0
					Routers[router_id]["compromised"] = isCompromised

				Routers[router_id]["lxcs"].append(lxc_id)
				Routers[router_id]["n_intf"] = Routers[router_id]["n_intf"] + 1


			elif router_1 != None and router_2 != None :

				if router_1.endswith("C") :
					router_1_id = int(router_1[1:-1])
					isCompromised = 1
				else :
					router_1_id = int(router_1[1:])
					isCompromised = 0
				

				if not router_1_id in Routers.keys() :
					Routers[router_1_id] = {}
					Routers[router_1_id]["lxcs"] = []
					Routers[router_1_id]["routers"] = []
					Routers[router_1_id]["n_intf"] = 0
					Routers[router_1_id]["nxt_intf"] = 0
					Routers[router_1_id]["compromised"] = isCompromised

				#router_2_id = int(router_2[1:])
				if router_2.endswith("C") :
					router_2_id = int(router_2[1:-1])
					isCompromised = 1
				else :
					router_2_id = int(router_2[1:])
					isCompromised = 0

				if not router_2_id in Routers.keys() :
					Routers[router_2_id] = {}
					Routers[router_2_id]["lxcs"] = []
					Routers[router_2_id]["routers"] = []
					Routers[router_2_id]["n_intf"] = 0
					Routers[router_2_id]["nxt_intf"] = 0
					Routers[router_2_id]["compromised"] = isCompromised

				if router_1_id == router_2_id :
					ERROR("Link cannot be established between the same router")

				if router_1_id < router_2_id :
					Routers[router_1_id]["routers"].append((router_2_id,delay))				
				else :
					Routers[router_2_id]["routers"].append((router_1_id,delay))
					
				Routers[router_1_id]["n_intf"] = Routers[router_1_id]["n_intf"] + 1
				Routers[router_2_id]["n_intf"] = Routers[router_2_id]["n_intf"] + 1

			else :
				ERROR("Link description syntax error")



			
	n_nodes = len(Lxcs.keys())
	nodes = Lxcs.keys()
	routers = Routers.keys()
	router_net_id = n_nodes

	#if n_nodes != exp_n_nodes + 1:
	#	ERROR("IDS not defined in Topology config")



	with open(curr_conf_file,"w") as f :
		f.write("total_timeline " + str(N_CPUS) + "\n")
		#f.write("total_timeline " + str(n_nodes + 1) + "\n")
		f.write("tick_per_second 6\n")
		f.write("run_time " + str(exp_run_time) + "\n")
		f.write("seed 1\n")
		f.write("log_dir " + exp_name + "\n\n")
		f.write("dilation [ TDF " + str(exp_tdf) + " ]\n")
		f.write("Net\n")
		f.write("[\n")
		f.write("	lxcConfig\n")
		f.write("	[\n")
		for node in xrange(1,n_nodes+1)  :
			if node == n_nodes :
				f.write("		settings [lxcNHI " + str(node-1) + ":0 _extends .dilation cmd " +  "\"python " + conf_directory + "/PLC_Config/udp_reader.py" + "\"]\n")
			else :
				f.write("		settings [ lxcNHI " + str(node-1) + ":0 _extends .dilation cmd " + "\"" + test_file + " -e 0" + " --node " + str(node-1) + Node[node]["script"] + "\"" + " ]\n")
		f.write("	]\n")
		curr_timeline = 1
	 	for node in xrange(1,n_nodes + 1)  :

	 		if node == n_nodes :
	 			assigned_timeline = 0
	 		elif node < math.pow(2,curr_timeline):
	 			assigned_timeline = curr_timeline - 1
	 		else:
	 			if node >= int(n_nodes/2) :
	 				if node % 2 == 0:
	 					assigned_timeline = 0
	 				else :
	 					assigned_timeline = curr_timeline
	 			else :
	 				assigned_timeline = curr_timeline
	 			curr_timeline = curr_timeline + 1

			f.write("	Net\n")
			f.write("	[\n")
			f.write("		id " + str(node-1) + "\n")
			f.write("		alignment " + str((node-1) % (N_CPUS - 1)) + "\n")
			#f.write("		alignment " + str(node-1) + "\n")
			#f.write("		alignment " + str(assigned_timeline) + "\n")
			f.write("		host\n")
			f.write("		[\n")
			f.write("			id 0\n")
			f.write("			_extends .dict.emuHost\n")
			f.write("		]\n")
			f.write("	]\n")

		f.write("\n")

	 	#for node in xrange(1,exp_n_nodes + 1) :
	 	#	if Node[node]["marked"] == False :
	 	#		add_links(Node,node,f)

		f.write("	Net\n")	
		f.write("	[\n")
		f.write("		id " + str(router_net_id) + "\n")
	 	#f.write("		alignment " + str(n_nodes) + "\n")
	 	f.write("		alignment " + str(N_CPUS - 1) + "\n")

	 	for router in routers :
	 		n_intf = Routers[router]["n_intf"]
	 		f.write("		router\n")
	 		f.write("		[\n")
	 		f.write("			id 	" + str(router) + "\n")
	 		if Routers[router]["compromised"] == 1 :
	 			f.write("			isCompromised 1\n")
	 			f.write("			_find	.dict.compromisedrouterGraph.graph\n")
	 		else :
	 			f.write("			_find .dict.routerGraph.graph\n")
	 		i = 0
	 		while i < n_intf :
	 			f.write("			interface [ id " + str(i) + "	_extends  .dict.1Gb ]\n")
	 			i = i + 1

	 		f.write("		]\n")


	 	f.write("	]\n\n")

	 	for node in nodes :
	 		router = Lxcs[node]["router"]
	 		nxt_free_intf = Routers[router]["nxt_intf"]
	 		link_delay = float(Lxcs[node]["delay"])/1000.0
	 		f.write("	link [ attach " + str(node) + ":0(0) attach " + str(router_net_id) + ":" + str(router) + "(" + str(nxt_free_intf) + ")" + " min_delay 1e-6 prop_delay " + str(link_delay) + " ]\n")
	 		Routers[router]["nxt_intf"] = Routers[router]["nxt_intf"] + 1

		for router in routers :
	 		neighbours = Routers[router]["routers"]
	 		for neighbour_id,delay in neighbours :
	 			link_delay = float(delay)/1000.0
	 			dst_nxt_free_intf = Routers[neighbour_id]["nxt_intf"]
	 			src_nxt_free_intf = Routers[router]["nxt_intf"]
	 			f.write("	link [ attach " + str(router_net_id) + ":" + str(router) + "(" + str(src_nxt_free_intf) + ") attach " + str(router_net_id) + ":" + str(neighbour_id) + "(" + str(dst_nxt_free_intf) + ")" + " min_delay 1e-6 prop_delay " + str(link_delay) + " ]\n")
	 			Routers[router]["nxt_intf"] = Routers[router]["nxt_intf"] + 1
	 			Routers[neighbour_id]["nxt_intf"] = Routers[neighbour_id]["nxt_intf"] + 1

				

		f.write("]\n")