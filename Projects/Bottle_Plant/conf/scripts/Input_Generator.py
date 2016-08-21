import sys
import os
import time
import math
import shared_sem
from shared_sem import *

Input_0_period = 0.1 # in seconds
Input_1_period = 0.2
Input_2_period = 0.5
conveyor_delay = 0.05
Max_Inputs = 5
N_levels = 1

def append_to_file(filename,data):
	if not os.path.isfile(filename):
		with open(filename,'w') as f:
			f.write( data + "\n")
	else:
		with open(filename,'a') as f:
			f.write( data + "\n")


# write your code here to generate in inputs for a given node after a given number of cycles
# this function gets  automatically called by the simulator for every cpu cycle
# INPUTS :  node_id	     : Node id in sumulation
#	    	cpu_cycle_number : number of elapsed cpu cycles
#	    	curr_outputs     : bytearray of current status of all cpu outputs
#           curr_inputs	     : bytearray of current status of all cpu inputs
			
# OUTPUTS : list of 1s and 0s for each input. The simulator will feed these inputs to the emulated PLC for the next cpu cycle.
def generate_inputs(Node_data,node_id,cpu_cycle_number,curr_outputs=None,curr_inputs=None) :

	node_id = node_id + 1

	if cpu_cycle_number <= 1:
		s_time = time.time()
		Node_data["start_time"] = s_time
		Node_data["n_inputs"] = 0
		Node_data["last_input0_time"] = s_time + 1.0
		Node_data["last_input1_time"] = s_time
		Node_data["last_input2_time"] = s_time
		Node_data["next_conn_2_time"] = -1
		Node_data["next_conn_3_time"] = -1
		Node_data["next_input0_time"] = -1
		init_shared_sem(2*node_id,2,1)
		init_shared_sem(2*node_id + 1,3,1)
		init_shared_sem(node_id,1,0)

		#with open(str(node_id) + ".txt",'w') as f:
		#		pass
		#with open(str(2*node_id) + ".txt",'w') as f:
		#		pass
		#with open(str(2*node_id + 1) + ".txt",'w') as f:
		#		pass


		return []


	in0 = 0
	in1 = 0
	in2 = 0

	# Generating input 0
	#print("Cycle no = ", cpu_cycle_number)

	curr_time = time.time()
	if node_id == 1 and curr_time - Node_data["last_input0_time"] > Input_0_period and Node_data["n_inputs"] < Max_Inputs :
		Node_data["last_input0_time"] = curr_time
		Node_data["n_inputs"] = Node_data["n_inputs"] + 1
		print("New input generated")
		in0 = 1

	if Node_data["next_conn_2_time"] != -1:
		res = acquire_shared_sem(2)
		if res == 1: # semaphore acquired
			print("Routed new job. Node_id = ", node_id, " to ", 2*node_id)
			fname = Node_data["conf_directory"] + "/logs/routing_logs/" + str(2*node_id) + ".txt"
			append_to_file(fname, str(curr_time + conveyor_delay))
			release_shared_sem(2)
			Node_data["next_conn_2_time"] = -1

	if Node_data["next_conn_3_time"] != -1:
		res = acquire_shared_sem(3)
		if res == 1: # semaphore acquired
			print("Routed new job. Node_id = ", node_id, " to ", 2*node_id+1)
			fname = Node_data["conf_directory"] + "/logs/routing_logs/" + str(2*node_id + 1) + ".txt"
			append_to_file(fname, str(curr_time + conveyor_delay))
			release_shared_sem(3)
			Node_data["next_conn_3_time"] = -1

	
	if int(curr_outputs[0]) == 1 :
		
		res = acquire_shared_sem(2)
		if res == 1: # semaphore acquired
			print("Routed new job. Node_id = ", node_id, " to ", 2*node_id)
			fname = Node_data["conf_directory"] + "/logs/routing_logs/" + str(2*node_id) + ".txt"
			append_to_file(fname, str(curr_time + conveyor_delay))
			release_shared_sem(2)
		else: 	# semaphore not acquired
			Node_data["next_conn_2_time"] = curr_time + conveyor_delay

	elif int(curr_outputs[0]) == 2 :
		res = acquire_shared_sem(3)
		if res == 1: # semaphore acquired
			print("Routed new job. Node_id = ", node_id, " to ", 2*node_id + 1)
			fname = Node_data["conf_directory"] + "/logs/routing_logs/" + str(2*node_id + 1) + ".txt"
			append_to_file(fname, str(curr_time + conveyor_delay))
			release_shared_sem(3)
		else:
			Node_data["next_conn_3_time"] = curr_time + conveyor_delay
		
	if Node_data["next_input0_time"] != -1 :
		if curr_time > Node_data["next_input0_time"] :
			in0 = 1
			Node_data["next_input0_time"] = -1

	elif os.path.isfile(Node_data["conf_directory"] + "/logs/routing_logs/" + str(node_id) + ".txt") and node_id != 1 :

		res = acquire_shared_sem(1)
		if res == 1:
			fname = Node_data["conf_directory"] + "/logs/routing_logs/" + str(node_id) + ".txt"

			with open(fname,'rw') as f:
				data = f.read().splitlines(True)
				if len(data) > 0:
					print "Read data = ", data
				if len(data) > 0 :
					if len(data[0]) > 0 :
						read_time = float(data[0])
						if curr_time > read_time:
							in0 = 1		# Generate input 0
						else:
							Node_data["next_input0_time"] = read_time
							with open(fname, 'w') as f:
								if len(data) > 1 :
									f.writelines(data[1:])
    		
    		release_shared_sem(1)


    # Generating input 1 (for all nodes except the leaves)
	if curr_time - Node_data["last_input1_time"] > Input_1_period and node_id < math.pow(2,N_levels) :
		Node_data["last_input1_time"] = curr_time
		in1 = 1


	# Generating input 2
	#if node_id >= math.pow(2,N_levels) and curr_time - Node_data["last_input2_time"] > Input_2_period:
	#	Node_data["last_input2_time"] = curr_time
	#	in2 = 1

	input_val = in0 + (2*in1) + (4*in2)
	input_array = curr_inputs
	if input_array != None :
		input_array[0] = input_val
		return input_array
	else:
		return []

