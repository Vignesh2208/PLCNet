import os
import sys


NR_SERIAL_DEVS  = 5
KERN_BUF_SIZE = 100
TX_BUF_SIZE = 100
RX_BUF_SIZE = 2*TX_BUF_SIZE


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
s3f_directory = root_directory + "/s3fnet-lxc"
s3f_base_directory  = s3f_directory + "/base"


definitions = """
#ifndef __S3FNETDEFINITIONS_H__
#define __S3FNETDEFINITIONS_H__

// This enables composite synchronization. see s3f.h
#define COMPOSITE_SYNC



#define LOGGING
#define DEFAULT_TDF 50
//#define TAP_DISABLED

""" 

definitions = definitions + "#define NR_SERIAL_DEVS " + str(NR_SERIAL_DEVS) + "\n"
definitions = definitions + "#define KERN_BUF_SIZE " + str(KERN_BUF_SIZE) + "\n"
definitions = definitions + "#define TX_BUF_SIZE " + str(TX_BUF_SIZE) + "\n"
definitions = definitions + "#define RX_BUF_SIZE " + str(RX_BUF_SIZE) + "\n"
definitions = definitions + "#define PATH_TO_S3FNETLXC  " + "\"" + s3f_directory + "\"\n"
definitions = definitions + "#define PATH_TO_READER_DATA " + "\"" + s3f_directory + "/data" + "\"\n"
definitions = definitions + """
//#define PROGRESS_FORCE

//#define LXC_INIT_DEBUG
//#define LXC_INDIVIDUAL_STATS
//#define ADVANCE_DEBUG

#ifdef PROGRESS_FORCE
#define PROGRESS_FLAG 1
#else
#define PROGRESS_FLAG 0
#endif



#endif /*__S3FNETDEFINITIONS_H__*/
"""

with open(s3f_base_directory + "/s3fnet-definitions.h","w") as  f:
	f.write(definitions)
