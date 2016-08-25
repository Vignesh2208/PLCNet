from __future__ import division, absolute_import, print_function, unicode_literals
import socket
import sys
import os
from awlsim.core.systemblocks.connection import *	
from awlsim.core.systemblocks.exceptions import *
from awlsim.common.compat import *
from awlsim.definitions import *

from awlsim.core.systemblocks.systemblocks import *
from awlsim.core.util import *
from awlsim.core.datablocks import *

from multiprocessing import Process, Condition, Lock  
from multiprocessing.managers import BaseManager  
from multiprocessing import Manager
import time, os  
from test import *

class numManager(BaseManager):  
	pass 

 

	

DEBUG = 0

name = (0, "MODBUS_PN", "Modbus Implementation")

valid_parameters = [
	"Remote_Port",
	"Local_Port",
	"Remote_Partner_Name",
	"Is_Server",
	"Single_Write_Enabled",
	"Data_Area_1",
	"Data_Area_2",
	"Data_Area_3",
	"Data_Area_4",
	"Data_Area_5",
	"Data_Area_6",
	"Data_Area_7",
	"Data_Area_8"
]

required_parameters = [

	"Remote_Port",
	"Local_Port",
	"Remote_Partner_Name",
	"Is_Server",
	"Single_Write_Enabled",
]
	
optional_parameters = [
	"Data_Area_1",
	"Data_Area_2",
	"Data_Area_3",
	"Data_Area_4",
	"Data_Area_5",
	"Data_Area_6",
	"Data_Area_7",
	"Data_Area_8"
]

NOT_STARTED = -1
RUNNING = 4
DONE = 0
CONN_TIMEOUT_ERROR = 1
RECV_TIMEOUT_ERROR = 2
SERVER_ERROR = 3
CLIENT_ERROR = 5

class SFB102(SFB):
	

	interfaceFields = {
		BlockInterfaceField.FTYPE_IN	: (
			BlockInterfaceField(name = "ID",
					    dataType = AwlDataType.makeByName("WORD")),
			#BlockInterfaceField(name = "DB_PARAM",
			#		    dataType = AwlDataType.makeByName("DB")),
			BlockInterfaceField(name = "DB_PARAM",
					    dataType = AwlDataType.makeByName("INT")),
			
			BlockInterfaceField(name = "RECV_TIME",
					    dataType = AwlDataType.makeByName("TIME")),
			BlockInterfaceField(name = "CONN_TIME",
					    dataType = AwlDataType.makeByName("TIME")),
			BlockInterfaceField(name = "ENQ_ENR",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "DISCONNECT",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "REG_KEY",
					    dataType = AwlDataType.makeByName("CHAR",[(1,256)])),

		),
		BlockInterfaceField.FTYPE_OUT	: (
			BlockInterfaceField(name = "LICENSED",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "BUSY",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "CONN_ESTABLISHED",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "DONE_NDR",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "ERROR",
					    dataType = AwlDataType.makeByName("BOOL")),
			BlockInterfaceField(name = "STATUS_MODBUS",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "STATUS_CONN",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "STATUS_FUNC",
					    dataType = AwlDataType.makeByName("CHAR",[(1,256)])),
			BlockInterfaceField(name = "IDENT_CODE",
					    dataType = AwlDataType.makeByName("CHAR",[(1,256)])),

		),
		BlockInterfaceField.FTYPE_INOUT	: (
			BlockInterfaceField(name = "UNIT",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "DATA_TYPE",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "START_ADDRESS",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "LENGTH",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "TI",
					    dataType = AwlDataType.makeByName("WORD")),
			BlockInterfaceField(name = "WRITE_READ",
					    dataType = AwlDataType.makeByName("BOOL")),
		),
	}


	def __init__(self,cpu) :
		SFB.__init__(self,cpu)
		self.connections = {}
		self.connection_objects = {}
		

		self.local_id = self.cpu.local_id
		conf_file = conf_directory + "/PLC_Config/Config_Node_" + str(self.local_id) + ".txt"
		lines = [line.rstrip('\n') for line in open(conf_file)]
		resolved_connection = False
		
		for line in lines :
			line = ' '.join(line.split())
			if not line.startswith("#") :
				line = line.split('=')
				if len(line) > 1 :
					parameter = line[0]
					value = line[1]
					if value.startswith("#") :
						raise AwlSimError("Invalid value in configuration file " + conf_file)
			
					value = value.split("#")
					value = value[0]
					parameter = parameter.replace(" ","")
					value = value.replace(" ","")

					if parameter == "Connection_ID" :
						resolved_connection = False
					
					if resolved_connection == False :
						if not parameter == "Connection_ID" :
							raise AwlSimError("Connection ID must be specified first for new connection in file "+ conf_file + " " + parameter)
						self.connections[int(value)] = {}
						last_connection_id = int(value)
						n_resolved_required_parameters = 0
						n_resolved_optional_parameters = 0
						resolved_connection = True
					elif parameter == "Connection_ID" :
						raise AwlSimError("Cannot start parsing new connection. Missing parameters for previos Connection " + last_connection_id)
					elif parameter not in valid_parameters :
						raise AwlSimError("Unrecognized parameter " + parameter + " in file " + conf_file)
					else :

						if parameter == "Remote_Port" :
							value = int(value)
						elif parameter == "Local_Port" :
							value = int(value)
						elif parameter == "Is_Server" :
							value = True if value == "True" else False
						elif parameter == "Single_Write_Enabled" :
							value = True if value == "True" else False
						
						if parameter in required_parameters :
							n_resolved_required_parameters = n_resolved_required_parameters + 1
						else :
							n_resolved_optional_parameters = n_resolved_optional_parameters + 1

						self.connections[last_connection_id][parameter] = value
						#if n_resolved_required_parameters == len(required_parameters) and n_resolved_optional_parameters >= 1 :
						#	resolved_connection = False														
		
		

		for connection_id in self.connections.keys() :
			remote_port = self.connections[connection_id]["Remote_Port"]
			local_port = self.connections[connection_id]["Local_Port"]
			is_server = self.connections[connection_id]["Is_Server"]
			single_write_enabled = self.connections[connection_id]["Single_Write_Enabled"]
			remote_host_name = self.connections[connection_id]["Remote_Partner_Name"]

			data_areas = []
			key_list = self.connections[connection_id].keys()
			for parameter in  key_list :
				if parameter not in required_parameters :
					data_areas.append(self.connections[connection_id][parameter])

			self.connection_objects[connection_id] = Connection(cpu,connection_id,remote_port,local_port,remote_host_name,is_server,single_write_enabled,data_areas)
			
		
		
		
	def resolve_hostname_to_ip(self,hostname) :
		return socket.gethostbyname(hostname)


	def __repr__(self):
		name = ""
		for connection_id in self.connection_objects.keys():
			name = name + str(connection_id) + ":"
		return name

	def close(self):
		print("Deleting SFB102 ...")
		for connection_id in self.connection_objects.keys():
			conn_obj = self.connection_objects[connection_id]
			conn_obj.close()


	def run(self):

		DB_X = self.fetchInterfaceFieldByName("DB_PARAM")
		connection_id = self.fetchInterfaceFieldByName("ID")
		if connection_id not in self.connection_objects.keys() :
			raise AwlSimError("Invalid connection ID " + str(connection_id))

		self.storeInterfaceFieldByName("LICENSED", True)

		if self.connection_objects[connection_id].initialised == False :
			#if DB_X.data_type.index in  self.cpu.dbs.keys() :
			#	parameter_db = self.cpu.dbs[DB_X.datatype.index]
			#else :
			#	parameter_db = DB(DB_X.data_type.index,codeBlock=None,permissions=(1 << 0 | 1 << 1))
			#	self.cpu.dbs[DB_X.datatype.index] = parameter_db

			if DB_X in  self.cpu.dbs.keys() :
				parameter_db = self.cpu.dbs[DB_X]
			else :
				parameter_db = DB(DB_X,codeBlock=None,permissions=(1 << 0 | 1 << 1))
				self.cpu.dbs[DB_X] = parameter_db

			self.connection_objects[connection_id].set_connection_param_fields(parameter_db)
			self.storeInterfaceFieldByName("BUSY", False)
			self.storeInterfaceFieldByName("CONN_ESTABLISHED", False)
			self.storeInterfaceFieldByName("DONE_NDR", True)
			self.storeInterfaceFieldByName("ERROR", False)
			self.storeInterfaceFieldByName("STATUS_MODBUS", 0x0)
			self.storeInterfaceFieldByName("STATUS_CONN", 0x0)
			
			if self.connection_objects[connection_id].is_server == True :
				self.storeInterfaceFieldByName("UNIT", 0)
				self.storeInterfaceFieldByName("DATA_TYPE", 0)
				self.storeInterfaceFieldByName("START_ADDRESS", 0)
				self.storeInterfaceFieldByName("LENGTH", 0)
				self.storeInterfaceFieldByName("TI", 0)
				self.storeInterfaceFieldByName("WRITE_READ", 0)
			else :
				pass
			self.connection_objects[connection_id].initialised = True

		else :
			ENQ_ENR = self.fetchInterfaceFieldByName("ENQ_ENR")
			KEY = self.fetchInterfaceFieldByName("REG_KEY")[2:]
			RECV_TIME = dwordToSignedPyInt(self.fetchInterfaceFieldByName("RECV_TIME"))/1000 # in seconds
			CONN_TIME = dwordToSignedPyInt(self.fetchInterfaceFieldByName("CONN_TIME"))/1000
			DISCONNECT = self.fetchInterfaceFieldByName("DISCONNECT")

			if RECV_TIME <= 0 :
				RECV_TIME = 5
			if CONN_TIME <= 0 :
				CONN_TIME = 5

			REG_KEY = ""
			for e in KEY :
				if e.decode('ascii') == 0 :
					break
				REG_KEY = REG_KEY + e.decode('ascii')

			if DEBUG == 1 :
				print("~~~~~~~~~~~~~~~~~~~~Inputs~~~~~~~~~~~~~~~~~~~")
				print("CONNECTION ID     = ",connection_id)
				print("CONN_TIME         = ",CONN_TIME)
				print("RECV_TIME         = ",RECV_TIME)
				print("DICONNECT         = ",DISCONNECT)
				print("ENQ_ENR           = ",ENQ_ENR)
				print("\n~~~~~~~~~~~~~~~~~~~Outputs~~~~~~~~~~~~~~~~~~~")

			connection = self.connection_objects[connection_id]
			if connection.is_server == True :

				connection.set_all_in_params_server(ENQ_ENR,DISCONNECT,RECV_TIME,CONN_TIME)
				#STATUS = connection.call_modbus_server()
				STATUS = connection.call_modbus_server_process()
				UNIT,TI,DATA_TYPE,WRITE_READ,LENGTH,START_ADDRESS,ERROR,BUSY,CONN_ESTABLISHED,STATUS_MODBUS,STATUS_CONN,STATUS_FUNC,IDENT_CODE = connection.get_all_out_params_server()
				self.storeInterfaceFieldByName("UNIT", UNIT)
				self.storeInterfaceFieldByName("DATA_TYPE", TI)
				self.storeInterfaceFieldByName("START_ADDRESS", START_ADDRESS)
				self.storeInterfaceFieldByName("LENGTH", LENGTH)
				self.storeInterfaceFieldByName("TI", TI)
				self.storeInterfaceFieldByName("WRITE_READ",WRITE_READ)
			else :
				UNIT = self.fetchInterfaceFieldByName("UNIT")
				DATA_TYPE = self.fetchInterfaceFieldByName("DATA_TYPE")
				START_ADDRESS = self.fetchInterfaceFieldByName("START_ADDRESS")
				LENGTH = self.fetchInterfaceFieldByName("LENGTH")
				TI = self.fetchInterfaceFieldByName("TI")
				WRITE_READ = self.fetchInterfaceFieldByName("WRITE_READ")

				
				connection.set_all_in_params_client(ENQ_ENR,DISCONNECT,RECV_TIME,CONN_TIME,UNIT,DATA_TYPE,WRITE_READ,TI,LENGTH,START_ADDRESS)
				#STATUS = connection.call_modbus_client()
				STATUS = connection.call_modbus_client_process()
				#STATUS = DONE
				ERROR,BUSY,CONN_ESTABLISHED,STATUS_MODBUS,STATUS_CONN,STATUS_FUNC,IDENT_CODE = connection.get_all_out_params_client()
				


			if STATUS != RUNNING :
				if connection.read_finish_status == 1: 
					self.storeInterfaceFieldByName("DONE_NDR", True)
					if DEBUG == 1 :
						print("DONE_NDR         = ", True)
					DONE_NDR = True
				else :
					self.storeInterfaceFieldByName("DONE_NDR",False)
					if DEBUG == 1 :
						print("DONE_NDR         = ", False)
					DONE_NDR = False
			else :
				self.storeInterfaceFieldByName("DONE_NDR",False)
				if DEBUG == 1 :
					print("DONE_NDR         = ", False)
				DONE_NDR = False

			self.storeInterfaceFieldByName("CONN_ESTABLISHED", CONN_ESTABLISHED)
			self.storeInterfaceFieldByName("ERROR", ERROR)
			self.storeInterfaceFieldByName("STATUS_MODBUS",STATUS_MODBUS)
			self.storeInterfaceFieldByName("STATUS_CONN", STATUS_CONN)
	
			value = [ord(elem) for elem in STATUS_FUNC]
			while len(value) != 256 :
				value.append(0)			
			
			self.storeInterfaceFieldByName("STATUS_FUNC",value)
			value = [ord(elem) for elem in IDENT_CODE]
			while len(value) != 256 :
				value.append(0)


			self.storeInterfaceFieldByName("IDENT_CODE",value)
			self.storeInterfaceFieldByName("BUSY", BUSY)

			if DEBUG == 1 :

				print("LICENSED         = ",True)
				print("CONN_ESTABLISHED = ", CONN_ESTABLISHED)
				print("BUSY             = ",BUSY)
				print("ERROR            = ",ERROR)
				print("STATUS_MODBUS    =  %x"%STATUS_MODBUS)
				print("STATUS_CONN      =  %x"%STATUS_CONN)

				if connection.is_server == True and  DONE_NDR == True:
					print("UNIT             = ",UNIT)
					print("TI               = ",TI)
					print("START_ADDRESS    = ",START_ADDRESS)
					print("LENGTH           = ",LENGTH)
					print("WRITE_READ       = ",WRITE_READ)
		
				print("#############################################")
			sys.stdout.flush()

		
