import os
import sys
import stat

N_CPUS = 6
NR_SERIAL_DEVS = 3
TX_BUF_SIZE = 100
TIMEKEEPER_BASE_DIR = "/home/vignesh/Desktop/Timekeeper"
KERN_BUF_SIZE = 100

scriptDir = os.path.dirname(os.path.realpath(__file__))
rootDir = scriptDir + "/.."

with open(scriptDir + "/../CONFIG.txt","r") as f:
	lines = f.readlines()
	lines = [line.rstrip('\n') for line in lines]
	for line in lines :
		line = ' '.join(line.split())
		if not line.startswith('#') :
			ls = line.split('=')
			
			if len(ls) >= 2 :

				Param = ls[0] 
				value = ls[1]

				if (not Param.startswith('#')) and (not value.startswith('#')) :

					if Param.startswith('N_CPUS') :
						N_CPUS = int(value)
					if Param.startswith('NR_SERIAL_DEVS') :
						NR_SERIAL_DEVS = int(value)
					if Param.startswith('TX_BUF_SIZE') :
						TX_BUF_SIZE = int(value)
					if Param.startswith('TIMEKEEPER_DIR') :
						TIMEKEEPER_BASE_DIR = value

TIMEKEEPER_ORIG_DIR = TIMEKEEPER_BASE_DIR + "/dilation-code"
TIMEKEEPER_DIR = "\"" + TIMEKEEPER_BASE_DIR + "/dilation-code"+ "\""
RX_BUF_SIZE = 2*TX_BUF_SIZE
tests_directory = rootDir + "/tests"
s3f_directory = rootDir + "/s3fnet-lxc"
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

with open(rootDir + "/scripts/create_sym_links.sh","w") as f:
	f.write("#!/bin/sh\n")
	f.write("rm -f experiment-data\n")
	f.write("rm -f "+ s3f_directory + "/dilation-code\n")
	f.write("ln -sf s3fnet-lxc/experiment-data experiment-data\n")
	f.write("ln -sf " + TIMEKEEPER_ORIG_DIR + " " + s3f_directory + "/dilation-code\n")

os.chmod(rootDir + "/scripts/create_sym_links.sh",stat.S_IRWXU)
os.chmod(rootDir + "/scripts/create_new_project.sh",stat.S_IRWXU)