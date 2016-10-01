from modbus_msg import *
from modbus_master import *
from modbus_utils import *

# Configuration settings

timeout = 5.0				# timeout for sending/receiving data over socket(secs)
connect_timeout = 5.0		# timeout for establishing connection to slave PLC (secs)
Memory_Type 	= "HOLDING_REG"	# Type of memory for operation: INPUT/INPUT_REG/HOLDING_REG/COIL
Access_Type = "READ"		# Type of operation: READ/WRITE
Memory_Start_Addr 	= 2		# Starting address of memory unit to perform operation. In this case Holding Register 2
N_Memory_Units 		= 2		# number of memory units to perform operation on. In this case 2 Holding registers starting from Register 2.
Slave_PLC_ID	= 0			# ID of the Slave PLC

TI 			= 0x10			# Transaction Identifier - to identify the request-response pair.
							# If connection is not closed, it must be incremented with each new
							# request.

coil_mem = {}				# Local Memory to store the response when read operations are performed on coils on the PLC
							# E.g if read op is performed on Coil 1 on the SLAVE PLC, the read value will be present in coil_mem["Coils_1"]
							# E.g if read op is performed for Input 1 on the SLAVE PLC, the read value will be present in coil_mem["Inputs_1"]

reg_mem = {}			    # Local Memory to store the response when read operations are performed on registers on the PLC
							# E.g if read op is performed on Holding Register 1 on the SLAVE PLC, the read value will be present in reg_mem["Holding_Register_1"]
							# E.g if read op is performed for Input Register 1 on the SLAVE PLC, the read value will be present in reg_mem["Input_Register_1"]


slavePLCConfigFile = os.path.dirname(os.path.realpath(__file__)) + "/PLC_Config/lxc" + str(Slave_PLC_ID) + "-0"
TCP_REMOTE_IP = hostname_to_ip(slavePLCConfigFile)
TCP_REMOTE_PORT = 502
RECV_BUFFER_SIZE = 4096



master = ModBusMaster(coil_mem,reg_mem)
msg_to_send,error = master.construct_request(Memory_Type,Access_Type,Memory_Start_Addr,N_Memory_Units,TI,Slave_PLC_ID)


client_socket = connect_to_slave(TCP_REMOTE_IP, TCP_REMOTE_PORT,connect_timeout)
send_command_to_slave(client_socket, msg_to_send, timeout)
recv_data = get_response_from_slave(client_socket,timeout,RECV_BUFFER_SIZE)

client_socket.close()
master.process_response(recv_data)
print "Read Register Value from PLC = ", reg_mem["Holding_Register_2"]
