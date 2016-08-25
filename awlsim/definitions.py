import os
import sys
import stat

## Config script. Do not modify ##
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


conf_directory = root_directory + "/Projects/Simple_PLC_Client_Server_multiple/conf"
tests_directory = root_directory + "/tests"
s3f_directory = root_directory + "/s3fnet-lxc"
s3f_base_directory  = s3f_directory + "/base"