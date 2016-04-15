import sys
import re

project_name = sys.argv[1]
find = "@PROJECT@"
replace = "Projects/" + project_name
with open ("template/config_parser.py.template", "r") as myfile:
     s=myfile.read()
ret = re.sub(find,replace, s)   
with open("config_parser.py","w") as f :
	f.write(ret)

with open ("template/definitions.py.template", "r") as myfile:
     s=myfile.read()
ret = re.sub(find,replace, s)   
with open("definitions.py","w") as f :
	f.write(ret)

