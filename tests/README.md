# PLCNet (Awlsim + S3FNet + TimeKeeper)
```
Awlsim is a free Step 7 compatible AWL/STL Soft-PLC written in Python.
Depending on the host machine and the Python interpreter used, it achieves
good performance of several thousand to millions of AWL/STL instructions
per second. German and English S7 AWL/STL mnemonics are supported.

This project integrates Awlsim-0.42 with TimeKeeper and S3FNet to achieve
high fidelity large scale PLC Network emulations. Additional support is 
included for simulating HMI devices which can send commands to slave PLCs.
It also allows the user to implement and test intrusion detection algorithms 
and subject the network to variety of man in the middle attack scenarios.

```

## Dependencies/Requirements
```
ubuntu-12.04 LTS or ubuntu-14.04.1 LTS (32 bit or 64 bit)
gcc 4.8, g++ 4.8
linux kernel-3.13.1
lxc-0.7.5
TimeKeeper 

```

## Installation Instructions
```

#install TimeKeeper (Refer TimeKeeper repository for instructions)

#clone PLCNet repository and rename the folder from PLCNet to awlsim-0.42

#install lxc-0.7.5 from https://linuxcontainers.org/lxc/downloads/

#edit CONFIG.txt to set parameters prior to Building
N_CPUS 	        # Number of VCPUs assigned for experiments. use lscpu command for help
NR_SERIAL_DEVS 	# Number of serial device interfaces assigned for each emulated PLC
TX_BUF_SIZE 	# Transmission buffer size (bytes) for assigned serial device interfaces
TIMEKEEPER_DIR	# Absolute path to the location of TimeKeeper source

# build source and install
sudo make fullbuild
```

## Creating and Launching new Projects
```
# create new project
sudo ./run.sh -n <Project name> -m

# make changes to the project
# set experiment configuration (refer Project README for further explanation)
  
  edit Projects/<Project name>/conf/exp_config

# set topology configuration (refer Project README for further explanation)
  topology configuration must specify an extra node (only in IP mode)
  for running a centralized intrusion detection monitor. 
  Please refer to Advanced Security Testing section for more details.

  edit Projects/<Project name>/conf/topology_config

# generate inputs to PLCs
  An input generator script located inside Projects/<Project name>/conf/
  scripts/InputGenerator.py is called for every PLC at the start of every cycle. 
  It can be used to set the inputs to the PLC for the current cycle. The arguments 
  passed to the script include the ID of the PLC, cycle number, PLC outputs from 
  previous cycle. The script should return inputs for the next cycle.

# build/load project 
sudo ./run.sh -n <Project name> -l

# run last built project
sudo ./run.sh -n <Project name> -r
```

## HMI Devices
```
The bundle also includes support for emulating HMI devices. Python modules are 
included which provide an API to emulate HMI devices. Three python modules 
modbus_msg, modbus_master and modbus_utils must be included by any emulated HMI
device.

An example project Simple_PLC_Client_HMI is included with the bundle.

Refer to Projects/Simple_PLC_Client_HMI/conf/hmi.py for details on usage.
```


## Advanced Security testing
```  
# Man in the Middle Attacks / Compromised Routers

1. Some routers in topology configuration can be reclared as compromised
   e.g Router 1 can be declared as compromised using notation R1C instead of R1
   e.g (1-R1C), 1

2. To implement MITM attacks,
   edit Projects/<Project name>/conf/cApp_inject_attack.cc
   edit Projects/<Project name>/conf/cApp_session.h

   These scripts will be called for all compromised routers. The ID of the 
   router can be obtained inside the script using getHost()->hostID and based
   on the ID, different actions can be taken in different compromised routers.

Rebuild/Reload Project after editing these files


# Centralized Log collector (or simulated Intrusion monitor)

There is also a provision to collect all packets exchanged between PLCs to a central 
location in IP Mode.This is done by a centralized log monitor script located in 
/Projects/<Project name>/conf/PLC_Conf/udp_reader.py. It is always started in an LXC with 
ID (Total Nodes + 1) by default. Topology Config must specify an extra node (ID: N_Nodes + 1), 
router (ID: N_Nodes + 1) pair (also referred to as IDS node-router pairs) which will by 
default run udp_reader.py script to collect and store logs. The user should connect the IDS 
router as desired to get traffic from all PLCs. The common logs collected will be stored in 
Projects/<Project name>/conf/logs/ids_monitor_log. Each node also logs its own sent and received
messages in IP and serial mode in seperate node log files which are also stored in the same directory.

edit Projects/<Project name>/conf/PLC_Config/udp_reader.py
```

## Additional Documentation
```
Please refer to documentation included with the repository for additional information related
to Siemens Step 7 programming instructions, Siemens Modbus API and other general information related
to Awlsim and ModBus

Awlsim-0.42 does not support all Step-7 instructions. For further clarifications, please visit the 
website: https://bues.ch/cms/automation/awlsim.html

This project is being maintained by Vignesh Babu (vig2208@gmail.com)
```
