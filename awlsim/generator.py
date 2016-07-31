import sys
import re
import os


project_name = sys.argv[1]


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


conf_directory = root_directory + "/Projects/"+ project_name + "/conf"
project_dir = root_directory + "/Projects/"+ project_name
tests_directory = root_directory + "/tests"
s3f_directory = root_directory + "/s3fnet-lxc"
s3f_base_directory  = s3f_directory + "/base"


find = "@PROJECT@"
replace = "Projects/" + project_name
with open ("template/config_parser.py.template", "r") as myfile:
     s=myfile.read()
ret = re.sub(find,replace, s)   
with open("config_parser.py","w") as f :
	f.write(ret)

with open ("template/create.template", "r") as myfile:
     s=myfile.read()

find = "@PROJECT_DIR@"
replace = project_dir

ret = re.sub(find,replace, s)   
with open(s3f_directory + "/lxc-scripts/create","w") as f :
	f.write(ret)

os.chmod(s3f_directory + "/lxc-scripts/create", 0777)

