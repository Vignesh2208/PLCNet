
from awlsim.common.compat import *

from awlsim.core.systemblocks.systemblocks import *
from awlsim.core.util import *

class Data_Area(object):
	def __init__(self,data_type,db,start,end):
		self.data_type = data_type
		self.db = db
		self.start = start
		self.end = end

class Connection_Params(object):

	def __init__(self,cpu) :
		self.block_length = 0x40
		self.cpu = cpu
		self.id = 0			# connection identifier	
		self.connection_type = 0x11
		self.active_est = False
		self.local_device_id = 1	# unused
		self.local_tsap_id_len = 2	# length of port number of local device
		self.rem_subnet_id_len = 0
		self.rem_staddr_len = 4
		self.rem_tsap_id_len = 2 	# length of port number of remote communication partner
		self.next_staddr_len = 0
		self.local_tsap_id = 502
		self.rem_subnet_id = [0]*6
		self.rem_staddr = ""
		self.rem_tsap_id = 502		
		self.next_staddr = 0
		self.spare = 0
		self.local_tsap_id_array = [0]*16
		self.rem_tsap_id_array = [0]*16
		self.rem_staddr_array = [0]*6
		self.next_staddr_array = [0]*6
		self.server_client = False
		self.single_write = False
		self.connect_at_startup = False
		self.data_area = {}
		i = 1
		while i <= 8 :
			self.data_area[i] = Data_Area(0,0,0,0)
			i = i + 1

	def set_connection_params(self,connection_id,remote_port,local_port,remote_host_ip,is_server,single_write_enabled) :

		self.id = int(connection_id)
		self.rem_tsap_id = int(remote_port)
		self.local_tsap_id = int(local_port)
		self.rem_staddr = remote_host_ip
		self.connect_at_startup = False
		self.server_client = is_server
		self.single_write = single_write_enabled
		if is_server == True :
			self.active_est = False
		else :
			self.active_est = True

		if self.active_est == True :
			self.local_tsap_id_len = 0
			self.rem_tsap_id_len = 2
			self.rem_tsap_id_array[0] = self.rem_tsap_id >> 8
			self.rem_tsap_id_array[1] = self.rem_tsap_id & 0x00FF
		else :
			self.local_tsap_id_len = 2
			self.rem_tsap_id_len = 0
			self.local_tsap_id_array[0] = self.local_tsap_id >> 8
			self.local_tsap_id_array[1] = self.local_tsap_id & 0x00FF

		
		temp_list = self.rem_staddr.split('.')
		i = 0
		for entry in temp_list :
			self.rem_staddr_array[i] = int(entry)
			i= i + 1

	def set_connection_param_fields(self,cpu,db) :


		# Add Connection parameter fields
		curr_dataType = AwlDataType.makeByName("WORD")
		curr_field_name = "block_length"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("WORD")
		curr_field_name = "id"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "connection_type"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BOOL")
		curr_field_name = "active_est"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "local_device_id"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "local_tsap_id_len"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "rem_subnet_id_len"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "rem_staddr_len"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "rem_tsap_id_len"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE")
		curr_field_name = "next_staddr_len"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE",[(1,16)])
		curr_field_name = "local_tsap_id"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE",[(1,6)])
		curr_field_name = "rem_subnet_id"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE",[(1,6)])
		curr_field_name = "rem_staddr"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE",[(1,16)])
		curr_field_name = "rem_tsap_id"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BYTE",[(1,6)])
		curr_field_name = "next_staddr"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		# Add Modbus Parameter fields
		
		curr_dataType = AwlDataType.makeByName("BOOL")
		curr_field_name = "server_client"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BOOL")
		curr_field_name = "single_write"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		curr_dataType = AwlDataType.makeByName("BOOL")
		curr_field_name = "connect_at_startup"
		db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

		i = 1
		while i <= 8 :
			curr_dataType = AwlDataType.makeByName("BYTE")
			curr_field_name = "data_area_" + str(i) + ".data_type"
			db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)
		
			curr_dataType = AwlDataType.makeByName("WORD")
			curr_field_name = "data_area_" + str(i) +  ".db"
			db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)

			curr_dataType = AwlDataType.makeByName("WORD")
			curr_field_name = "data_area_" + str(i) + ".start"
			db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)
	
			curr_dataType = AwlDataType.makeByName("WORD")
			curr_field_name = "data_area_" + str(i) + ".end"
			db.struct.addFieldNaturallyAligned(self.cpu, curr_field_name, curr_dataType, initBytes = None)		
			
			i = i + 1

		db.allocate()	# create an AwlStructInstance
			

	def store_connection_params(self,cpu,db) :
		
		db.structInstance.setFieldDataByName("block_length", self.block_length)
		db.structInstance.setFieldDataByName("id", self.id)
		db.structInstance.setFieldDataByName("connection_type", self.connection_type)
		db.structInstance.setFieldDataByName("active_est", 1 if self.active_est == True else 0)
		db.structInstance.setFieldDataByName("local_device_id", self.local_device_id)
		db.structInstance.setFieldDataByName("local_tsap_id_len", self.local_tsap_id_len)
		db.structInstance.setFieldDataByName("rem_subnet_id_len", self.rem_subnet_id_len)
		db.structInstance.setFieldDataByName("rem_staddr_len", self.rem_staddr_len)
		db.structInstance.setFieldDataByName("rem_tsap_id_len", self.rem_tsap_id_len)
		db.structInstance.setFieldDataByName("next_staddr_len", self.next_staddr_len)
		db.structInstance.setFieldDataByName("server_client", 1 if self.server_client == True else 0)
		db.structInstance.setFieldDataByName("single_write", 1 if self.single_write == True else 0)
		db.structInstance.setFieldDataByName("connect_at_startup", 1 if self.connect_at_startup == True else 0)

		i = 1
		while i <= 16 :
			fieldName = "rem_tsap_id[" + str(i) + "]"
			db.structInstance.setFieldDataByName(fieldName, self.rem_tsap_id_array[i-1])
			fieldName = "local_tsap_id[" + str(i) + "]"
			db.structInstance.setFieldDataByName(fieldName, self.local_tsap_id_array[i-1])
			
			if i <= 6 :
				fieldName = "rem_staddr[" + str(i) + "]"
				db.structInstance.setFieldDataByName(fieldName, self.rem_staddr_array[i-1])
				fieldName = "next_staddr[" + str(i) + "]"
				db.structInstance.setFieldDataByName(fieldName, self.next_staddr_array[i-1])
				fieldName = "rem_subnet_id[" + str(i) + "]"
				db.structInstance.setFieldDataByName(fieldName, self.rem_subnet_id[i-1])

			if i <= 8 :
				fieldName = "data_area_" + str(i) + ".data_type"
				db.structInstance.setFieldDataByName(fieldName, self.data_area[i].data_type)
				fieldName = "data_area_" + str(i) + ".db"
				db.structInstance.setFieldDataByName(fieldName, self.data_area[i].db)
				fieldName = "data_area_" + str(i) + ".start"
				db.structInstance.setFieldDataByName(fieldName, self.data_area[i].start)
				fieldName = "data_area_" + str(i) + ".end"
				db.structInstance.setFieldDataByName(fieldName, self.data_area[i].end)
			i = i + 1



	def load_connection_params(self,db) :
		
		self.block_length = db.structInstance.getFieldDataByName("block_length")
		self.id = db.structInstance.getFieldDataByName("id")			# connection identifier	
		self.connection_type = db.structInstance.getFieldDataByName("connection_type")
		self.active_est = db.structInstance.getFieldDataByName("active_est")
		self.local_device_id = db.structInstance.getFieldDataByName("local_device_id")	# unused
		self.local_tsap_id_len = db.structInstance.getFieldDataByName("local_tsap_id_len")	# length of port number of local device
		self.rem_subnet_id_len = db.structInstance.getFieldDataByName("rem_subnet_id_len")
		self.rem_staddr_len = db.structInstance.getFieldDataByName("rem_staddr_len")
		self.rem_tsap_id_len = db.structInstance.getFieldDataByName("rem_tsap_id_len") 	# length of port number of remote communication partner
		self.next_staddr_len = db.structInstance.getFieldDataByName("next_staddr_len")
		self.local_tsap_id = db.structInstance.getFieldDataByName("local_tsap_id")
		self.spare = 0
		self.server_client = db.structInstance.getFieldDataByName("server_client")
		self.single_write = db.structInstance.getFieldDataByName("single_write")
		self.connect_at_startup = db.structInstance.getFieldDataByName("connect_at_startup")

		i = 1
		while i <= 16 :
			fieldName = "rem_tsap_id[" + str(i) + "]"
			self.rem_tsap_id_array[i-1] = db.structInstance.getFieldDataByName(fieldName)
			fieldName = "local_tsap_id[" + str(i) + "]"
			self.local_tsap_id_array[i-1] = db.structInstance.getFieldDataByName(fieldName)
			
			if i <= 6 :
				fieldName = "rem_staddr[" + str(i) + "]"
				self.rem_staddr_array[i-1] = db.structInstance.getFieldDataByName(fieldName)
				fieldName = "next_staddr[" + str(i) + "]"
				self.next_staddr_array[i-1] = db.structInstance.getFieldDataByName(fieldName)
				fieldName = "rem_subnet_id[" + str(i) + "]"
				self.rem_subnet_id[i-1] = db.structInstance.getFieldDataByName(fieldName)

			if i <= 8 :
				fieldName = "data_area_" + str(i) + ".data_type"
				self.data_area[i].data_type = db.structInstance.getFieldDataByName(fieldName)
				fieldName = "data_area_" + str(i) + ".db"
				self.data_area[i].db = db.structInstance.getFieldDataByName(fieldName)
				fieldName = "data_area_" + str(i) + ".start"
				self.data_area[i].start = db.structInstance.getFieldDataByName(fieldName)
				fieldName = "data_area_" + str(i) + ".end"
				self.data_area[i].end = db.structInstance.getFieldDataByName(fieldName)

			i = i + 1


		self.rem_staddr = ""
		self.local_tsap_id = 0
		self.rem_tsap_id = 0
		if self.active_est == 1 :
			self.rem_tsap_id = (self.rem_tsap_id_array[0] << 8 ) | self.rem_tsap_id_array[1]  
		else :
			self.local_tsap_id = (self.local_tsap_id_array[0] << 8 ) | self.local_tsap_id_array[1]  

		i = 0
		while i < 4 :
			self.rem_staddr = self.rem_staddr + "." + str(int(self.rem_staddr_array[i]))
			i = i + 1
		self.rem_staddr = self.rem_staddr[1:]

	def print_connection_params(self) :
 
		print("Block Length = ", self.block_length)
		print("Connection Type = ", self.connection_type)
		print("Active Establishment = ", self.active_est)
		print("Remote Port Number = ",self.rem_tsap_id)
		print("Local Port Number = ", self.local_tsap_id)
		print("Remote Partner IP = ", self.rem_staddr)
		print("Server or Client = ", self.server_client)
		print("Single Write Enabled = ", self.single_write)
		print("Connect at Startup Enabled = ", self.connect_at_startup)

		i = 1
		while i <= 8  :
			print("Data_Area -> data_type = ",self.data_area[i].data_type)
			print("Data_Area -> db = ",self.data_area[i].db)
			print("Data_Area -> start = ",self.data_area[i].start)
			print("Data_Area -> end = ",self.data_area[i].end)
			i = i + 1
