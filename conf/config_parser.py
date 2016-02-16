import os
import sys


def ERROR(string) :
	print string
	sys.exit(0)

def add_links(Node,node_id,file_pointer) :
	Node[node_id]["marked"] = True
	for connection in Node[node_id]["connections"] :
		curr_conn_params = Node[node_id]["connections"][connection]
		remote_partner = curr_conn_params[2]
		if Node[remote_partner]["marked"] == False :
			Node[remote_partner]["marked"] = True
			file_pointer.write("	link [ attach " + str(node_id - 1) + ":0(0) attach " + str(remote_partner-1) + ":0(0) _extends .dict.link_delay_1ms ]\n")
			add_links(Node,remote_partner,file_pointer)

script_path = os.path.dirname(os.path.realpath(__file__))
script_path_list = script_path.split('/')

root_directory = "/"
for entry in script_path_list :
	
	if entry == "awlsim-0.42" :
		root_directory = root_directory + "awlsim-0.42"
		break
	else :
		if len(entry) > 0 :
			root_directory = root_directory + entry + "/"


conf_directory = root_directory + "/conf"
tests_directory = root_directory + "/tests"
test_file = tests_directory + "/run.sh"

conf_file = conf_directory + "/Config.txt"
lines = [line.rstrip('\n') for line in open(conf_file)]


valid_exp_config_params = [
"Run_Time",
"Dilation_Factor",
"Experiment_Name",
"Number_Of_Nodes"
]

resolved_general_config = False
resolving_general_config = False
resolved_awl_script_config = False
resolving_awl_script_config = False
resolved_data_area_config = False
resolving_data_area_config = False
prev_resolve_completed = True
resolved_exp_config = False
resolving_exp_config = False
Node = {}
for line in lines :
	line = ' '.join(line.split())
	if not line.startswith("#") and len(line) > 1:
		print line

		if line == "START_OF_EXPERIMENT_CONFIGURATION" :
			resolving_exp_config = True
			prev_resolve_completed = False
		elif line == "START_OF_GENERAL_CONFIGURATION" :
			if prev_resolve_completed == False or resolved_exp_config == False:
				ERROR("Did not finish resolving previous configuration block")

			prev_resolve_completed = False
			resolving_general_config = True
		elif line == "START_OF_AWL_SCRIPT_CONFIGURATION" :
			if prev_resolve_completed == False or resolved_general_config == False:
				ERROR("Did not finish resolving previous configuration block")

			prev_resolve_completed = False
			resolving_awl_script_config = True
		elif line == "START_OF_DATA_AREA_CONFIGURATION" :
			if prev_resolve_completed == False or resolved_awl_script_config == False:
				ERROR("Did not finish resolving previous configuration block")

			prev_resolve_completed = False
			resolving_data_area_config = True
		
		elif line == "END_OF_EXPERIMENT_CONFIGURATION" :
			print "Done"
			resolving_exp_config = False
			resolved_exp_config = True
			prev_resolve_completed = True					

		elif line == "END_OF_GENERAL_CONFIGURATION" :
			if resolving_general_config != True :
				ERROR("Unexpected End of configuration block")
			prev_resolve_completed = True
			resolving_general_config = False
			resolved_general_config = True
		elif line == "END_OF_AWL_SCRIPT_CONFIGURATION" :
			if resolving_awl_script_config != True :
				ERROR("Unexpected End of configuration block")
			prev_resolve_completed = True
			resolving_awl_script_config = False
			resolved_awl_script_config = True
			line = line.split(',')
		elif line == "END_OF_DATA_AREA_CONFIGURATION" :
			if resolving_data_area_config != True :
				ERROR("Unexpected End of configuration block")
			prev_resolve_completed = True
			resolving_data_area_config = False
			resolved_data_area_config = True
		elif resolving_exp_config == True :
			line = line.replace(" ","")
			line = line.split("=")
			if len(line) > 1 :
				parameter = line[0]
				value = line[1]
				if parameter not in valid_exp_config_params :
					ERROR("Unrecognized experiement config parameter")
				if parameter == "Run_Time" :
					exp_run_time = float(value)
				if parameter == "Experiment_Name":
					exp_name = value
				if parameter == "Dilation_Factor" :
					exp_tdf = int(value)
				if parameter == "Number_Of_Nodes" :
					exp_n_nodes = int(value)
		else :
			line = line.split(',')
			length = len(line)
			if line[length -1] == '' :
				line.pop()
			if resolving_general_config == True:
				if len(line) != 6 :
					ERROR("Missing general configration params")
				node_id = int(line[0])
				connection_id = int(line[1])
				remote_port = int(line[2])
				local_port = int(line[3])
				remote_partner = int(line[4])

				if node_id <= 0 or node_id > exp_n_nodes :
					ERROR("Invalid Node ID")

				if "True" in line[5] :
					is_server = 1
				else :
					is_server = 0
				if node_id not in Node.keys() :
					Node[node_id] = {}
					Node[node_id]["connections"] = {}
					Node[node_id]["script"] = ""
				Node[node_id]["connections"][connection_id] = [remote_port,local_port,remote_partner,is_server]
			elif resolving_awl_script_config == True:
				if len(line) != 2 :
					ERROR("Missing AWL script configuration params")
				node_id = int(line[0])
				if node_id <= 0 or node_id > exp_n_nodes :
					ERROR("Invalid Node ID")
				path = line[1]
				if node_id not in Node.keys() :
					Node[node_id] = {}
					Node[node_id]["connections"] = {}
					Node[node_id]["script"] = ""
				if Node[node_id]["script"] != "" :
					ERROR("Two Awl Scripts assigned for node")
				Node[node_id]["script"] = path

			elif resolving_data_area_config == True:
				if len(line) < 3 :
					ERROR("Missing data area config params")

				node_id = int(line[0])
				if node_id <= 0 or node_id > exp_n_nodes :
					ERROR("Invalid Node ID")
				connection_id = int(line[1])
				if node_id not in Node.keys() :
					Node[node_id] = {}
					Node[node_id]["connections"] = {}
					Node[node_id]["script"] = ""
					Node[node_id]["connections"][connection_id] = []
				i = 2
				while i < len(line) :
					params = line[i]
					params = params[2:len(line[i])-1].split(':')
					data_type = int(params[0])
					db = int(params[1])
					start = int(params[2])
					end = int(params[3])
					Node[node_id]["connections"][connection_id].append((data_type,db,start,end))
					i = i + 1

if resolved_exp_config == False or resolved_general_config == False or resolved_awl_script_config == False or resolved_data_area_config == False :
	ERROR("All configurations have not been resolved. ERROR")

if exp_name == None or exp_tdf == None or exp_run_time == None or exp_n_nodes == None :
	ERROR("Experiment config parameters missing")

print "Run_Time : ", exp_run_time
print "Exp_Name : ", exp_name
print "TDF	: ", exp_tdf
print "Number Of Nodes : ", exp_n_nodes

if len(Node.keys()) != exp_n_nodes :
	ERROR("Missing some node configuration information")

#for node in Node.keys() :
#	print "Node : ",node
#	print "Node[connections]", Node[node]["connections"]
#	print "Node[script]", Node[node]["script"]


node = 1
for node in xrange(1,exp_n_nodes + 1) :
	Node[node]["marked"] = False
	curr_conf_file = conf_directory + "/PLC_Config/Config_Node_" + str(node - 1) + ".txt"
	with open(curr_conf_file,"w") as f :
		for connection in Node[node]["connections"].keys() :
			curr_conn_params = Node[node]["connections"][connection]
			remote_port = curr_conn_params[0]
			local_port = curr_conn_params[1]
			remote_partner = curr_conn_params[2]
			if curr_conn_params[3] == 1 :
				is_server = "True"
			else :
				is_server = "False"
			f.write("Connection_ID		=	" + str(connection) + "\n")
			f.write("Remote_Port		=	" + str(remote_port) + "\n")
			f.write("Local_Port		=	" + str(local_port) + "\n")
			f.write("Remote_Partner_Name	=	" + str(remote_partner - 1) + "\n")
			f.write("Is_Server		=	" +	is_server + "\n")
			f.write("Single_Write_Enabled	=	True\n")
			i = 4
			while i < len(curr_conn_params) :
				(data_type,db,start,end) = curr_conn_params[i]
				f.write("Data_Area_" + str(i-3) + "		=	" + str(data_type) + "," + str(db) + "," + str(start) + "," + str(end) + "\n")
				i = i + 1
			f.write("\n")


curr_conf_file = conf_directory + "/PLC_Config/test.dml"
Links = {}
with open(curr_conf_file,"w") as f :
	f.write("total_timeline " + str(len(Node.keys())) + "\n")
	f.write("tick_per_second 6\n")
	f.write("run_time " + str(exp_run_time) + "\n")
	f.write("seed 1\n")
	f.write("log_dir " + exp_name + "\n\n")
	f.write("dilation [ TDF " + str(exp_tdf) + " ]\n")
	f.write("Net\n")
	f.write("[\n")
	f.write("	lxcConfig\n")
	f.write("	[\n")
	for node in xrange(1,exp_n_nodes+1)  :
		f.write("		settings [ lxcNHI " + str(node-1) + ":0 _extends .dilation cmd " + "\"" + test_file + " --node " + str(node-1) + Node[node]["script"] + "\"" + " ]\n")

	f.write("	]\n")
	for node in xrange(1,exp_n_nodes + 1)  :
		f.write("	Net\n")
		f.write("	[\n")
		f.write("		id " + str(node-1) + "\n")
		f.write("		alignment " + str(node-1) + "\n")
		f.write("		host\n")
		f.write("		[\n")
		f.write("			id 0\n")
		f.write("			_extends .dict.emuHost\n")
		f.write("		]\n")
		f.write("	]\n")

	f.write("\n")
	for node in xrange(1,exp_n_nodes + 1) :
		if Node[node]["marked"] == False :
			add_links(Node,node,f)	
			

	f.write("]\n")
