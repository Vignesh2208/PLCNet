import os
import sys

N_CPUS = 6
NR_SERIAL_DEVS = 3
TX_BUF_SIZE = 100
TIMEKEEPER_DIR = "/home/vignesh/Desktop/Timekeeper/dilation-code"
KERN_BUF_SIZE = 100

with open("CONFIG.txt","r") as f:
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
						TIMEKEEPER_DIR = value


with open("awlsim/template/init.template","r") as f:
	definitions_template_str = f.read()


with open("awlsim/template/definitions.py.template","w") as f :
	f.write("import os\n")
	f.write("import sys\n")
	f.write("import stat\n\n")
	f.write("N_CPUS = " + str(N_CPUS) + "\n")
	f.write("TX_BUF_SIZE = " + str(TX_BUF_SIZE) + "\n")
	f.write("RX_BUF_SIZE = 2*TX_BUF_SIZE\n")
	f.write("KERN_BUF_SIZE = " + str(KERN_BUF_SIZE) + "\n")
	f.write("NR_SERIAL_DEVS = " + str(NR_SERIAL_DEVS) + "\n")
	f.write("TIMEKEEPER_DIR = " + str(TIMEKEEPER_DIR) + "\n\n")
	f.write(definitions_template_str)

