import sys
import os
import time
import math

Input_0_period = 0.1 # 100ms
Input_1_period = 0.1
Input_2_period = 0.5
N_levels = 1



# write your code here to generate in inputs for a given node after a given number of cycles
# this function gets  automatically called by the simulator for every cpu cycle
# INPUTS :  node_id	     : Node id in sumulation
#	    cpu_cycle_number : number of elapsed cpu cycles
#	    curr_outputs     : bytearray of current status of all cpu outputs
#           curr_inputs	     : bytearray of current status of all cpu inputs
# OUTPUTS : list of 1s and 0s for each input. The simulator will feed these inputs to the emulated PLC for the next cpu cycle.
def generate_inputs(Node_data,node_id,cpu_cycle_number,curr_outputs=None,curr_inputs=None) :

	node_id = node_id + 1

	if cpu_cycle_number == 1:
		s_time = time.time()
		Node_data["start_time"] = s_time
		Node_data["last_input0_time"] = s_time
		Node_data["last_input1_time"] = s_time
		Node_data["last_input2_time"] = s_time
		Node_data["last_input3_time"] = s_time
		with open(str(node_id) + ".txt",'w') as f:
				pass
		with open(str(2*node_id) + ".txt",'w') as f:
				pass
		with open(str(2*node_id + 1) + ".txt",'w') as f:
				pass


		return []


	in0 = 0
	in1 = 0
	in2 = 0

	# Generating input 0
	#print("Cycle no = ", cpu_cycle_number)

	curr_time = time.time()
	if node_id == 1 and curr_time - Node_data["last_input0_time"] > Input_0_period:
		Node_data["last_input0_time"] = curr_time
		print("New input generated")
		in0 = 1
	
	if int(curr_outputs[0]) == 1 :
		print("Routed new job. Node_id = ", node_id, " to ", 2*node_id)
		if not os.path.isfile(str(node_id*2) + ".txt"):
			with open(str(node_id*2) + ".txt",'w') as f:
				pass

		with open(str(node_id*2) + ".txt",'a') as f:
			f.write(str(curr_time + 0.5*Input_0_period) + "\n")	# time of next input at 2*node_id
	elif int(curr_outputs[0]) == 2 :
		print("Routed new job. Node_id = ", node_id, " to ", 2*node_id + 1)
		if not os.path.isfile(str(node_id*2 + 1) + ".txt"):
			with open(str(node_id*2) + ".txt",'w') as f:
				pass
		with open(str(node_id*2 + 1) + ".txt",'a') as f:
			f.write(str(curr_time + 0.5*Input_0_period) + "\n")	# time of next input at 2*node_id

	if os.path.isfile(str(node_id) + ".txt") and node_id != 1 :
		with open(str(node_id) + ".txt",'r') as f:
			data = f.read().splitlines(True)
			#if len(data) > 0:
				#print("Data len = ", len(data)," Node_id = ", node_id)
		if len(data) > 0 :
			if len(data[0]) > 0 :
				read_time = float(data[0])
				if read_time > curr_time:
					in0 = 1		# Generate input 0
					with open(str(node_id) + ".txt", 'w') as f:
						if len(data) > 1 :
							f.writelines(data[1:])
    					


    # Generating input 1

	if curr_time - Node_data["last_input1_time"] > Input_1_period and node_id == 1:
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

