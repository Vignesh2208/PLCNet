from definitions import NR_SERIAL_DEVS

def parse_modbus_serial_topology(conf_directory,curr_conf_file,test_file,topology_file,exp_run_time,exp_name,exp_tdf,exp_n_nodes,Node,N_CPUS):

	Lxcs = {}
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

			node_1 = int(link_ls[0])
			node_2 = int(link_ls[1])

			if not node_1 in Lxcs.keys() :
				Lxcs[node_1] = {}
				Lxcs[node_1]["lxcs"] = []
				Lxcs[node_1]["n_intf"] = 0
				Lxcs[node_1]["nxt_intf"] = 0

			if not node_2 in Lxcs.keys() :
				Lxcs[node_2] = {}
				Lxcs[node_2]["lxcs"] = []
				Lxcs[node_2]["n_intf"] = 0
				Lxcs[node_2]["nxt_intf"] = 0

			if node_1 == node_2 :
				ERROR("Link cannot be established between the same node")
			if node_1 < node_2 :
				Lxcs[node_1]["lxcs"].append((node_2,delay))				
			else :
				Lxcs[node_2]["lxcs"].append((node_1,delay))

					
			Lxcs[node_1]["n_intf"] = Lxcs[node_1]["n_intf"] + 1
			Lxcs[node_2]["n_intf"] = Lxcs[node_2]["n_intf"] + 1
			
			if Lxcs[node_1]["n_intf"] > NR_SERIAL_DEVS or Lxcs[node_2]["n_intf"] > NR_SERIAL_DEVS :
				ERROR("Exceeded maximum number of serial devs")


			
	n_nodes = len(Lxcs.keys())
	nodes = Lxcs.keys()
	
	if n_nodes != exp_n_nodes :
		ERROR("Number of nodes defined in Experiment config does not match the number of nodes defined in Topology config")
	

		
	with open(curr_conf_file,"w") as f :
		#f.write("total_timeline " + str(n_nodes) + "\n")
		#f.write("total_timeline " + str(N_CPUS-1) + "\n")
		f.write("total_timeline 2" + "\n")
		f.write("tick_per_second 6\n")
		f.write("run_time " + str(exp_run_time) + "\n")
		f.write("seed 1\n")
		f.write("log_dir " + exp_name + "\n\n")
		f.write("dilation [ TDF " + str(exp_tdf) + " ]\n")
		
		#f.write("emuSerialHost\n")
		#f.write("[\n")
		#f.write("	isEmulated 1\n")
		#i = 0
		#while i < NR_SERIAL_DEVS :
		#	f.write("	interface [ " + str(i) + " _extends .dict.1Mb\n")
		#	f.write("			ProtocolSession [name mac use \"s3f.os.dummymac\" ]\n")
		#	f.write("	]\n")
		#	i = i + 1
		#f.write("	_find .dict.emuHostSerialGraph.graph\n")
		#f.write("]\n\n")
		


		f.write("Net\n")
		f.write("[\n")
		f.write("	lxcConfig\n")
		f.write("	[\n")
		for node in xrange(1,exp_n_nodes+1)  :
			f.write("		settings [ lxcNHI " + str(node-1) + ":0 _extends .dilation cmd " + "\"" + test_file + " -e 1" + " --node " + str(node-1) + Node[node]["script"] + "\"" + " ]\n")
		f.write("	]\n")
	 	for node in nodes :
			f.write("	Net\n")
			f.write("	[\n")
			f.write("		id " + str(node) + "\n")
			#f.write("		alignment " + str(node) + "\n")
			f.write("		alignment " + str(node % (N_CPUS-1)) + "\n")
			f.write("		host\n")
			f.write("		[\n")
			f.write("			id 0\n")
			f.write("			isEmulated 1\n")
			i = 0
			while i < Lxcs[node]["n_intf"] :
				f.write("			interface [ id " + str(i) + " _extends .dict.1Mb\n")
				f.write("				ProtocolSession [name mac use \"s3f.os.dummymac\" ]\n")
				f.write("			]\n")
				i = i + 1
			f.write("			_find .dict.emuHostSerialGraph.graph\n")
			f.write("		]\n")			
			f.write("	]\n")

		f.write("\n")

	 	
	 	for node in nodes :
	 		neighbours = Lxcs[node]["lxcs"]
	 		for neighbour_id,delay in neighbours :
	 			link_delay = float(delay)/1000.0
	 			dst_nxt_free_intf = Lxcs[neighbour_id]["nxt_intf"]
	 			src_nxt_free_intf = Lxcs[node]["nxt_intf"]
	 			f.write("	link [ attach " + str(node) + ":" + str(0) + "(" + str(src_nxt_free_intf) + ") attach " + str(neighbour_id) + ":" + str(0) + "(" + str(dst_nxt_free_intf) + ")" + " min_delay 1e-6 prop_delay " + str(link_delay) + " ]\n")
	 			Lxcs[node]["nxt_intf"] = Lxcs[node]["nxt_intf"] + 1
	 			Lxcs[neighbour_id]["nxt_intf"] = Lxcs[neighbour_id]["nxt_intf"] + 1

				

		f.write("]\n")