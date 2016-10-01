

from awlsim.common.compat import *

from awlsim.core.systemblocks.systemblocks import *
from awlsim.core.util import *

msg_params = {
	"startingAddress" : 0,
	"numberOfPoints"  : 0,
	"presetAddress"   : 0,
	"numberPreset"    : 0,
}

modbus_area_data_types = {
	0 : "Unused",
	1 : "Coils",
	2 : "Inputs",
	3 : "Holding_Register",
	4 : "Input_Register",
}
NO_ERROR = 0
ILLEGAL_FUNCTION = 1
ILLEGAL_DATA_ADDRESS = 2
ILLEGAL_DATA_VALUE = 3


class ModBusRequestMessage(object) :


	def __init__(self,connection,functionCode,slaveAddress,t_id) :
		self.slaveAddress = slaveAddress
		self.functionCode = functionCode
		self.transaction_id = t_id
		self.startingAddressHi = 0
		self.startingAddressLo = 0
		self.numberOfPointsHi = 0
		self.numberOfPointsLo = 0
		self.presetAddressHi = 0
		self.presetAddressLo = 0
		self.presetDataHi = []
		self.presetDataLo = []
		self.numberPresetHi = 0
		self.numberPresetLo = 0
		self.byteCount = 0	
		self.msgCRC = 0
		self.connection = connection
		self.cpu = connection.cpu

	def read_coil_status(self,presetAddress) :
		i = 1
		found = False
		type = 1
		while i <= 8 :
			start = self.connection.connection_params.data_area[i].start
			end = self.connection.connection_params.data_area[i].end
			dbNumber = self.connection.connection_params.data_area[i].db
			data_area = i
				
			if self.connection.connection_params.data_area[i].data_type == 1 and presetAddress >= start and presetAddress <= end : # coil data Type
				found = True
				break
			i = i + 1				
		if found == False :
			raise AwlSimError("Coil Memory not defined. Cannot preset coils")
		coil_word = (presetAddress - start)/16 
		coil_bit_in_word = (presetAddress - start)%16
		if type == 1 :
			coil_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Coils_" +  str(coil_word))
		if type == 2 :
			coil_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Inputs_" +  str(coil_word))

		coil_status = (coil_word_data & (1 << coil_bit_in_word)) >> coil_bit_in_word
		return coil_status
	
	def read_holding_register(self,presetAddress) :
		i = 1
		found = False
		type = 3
		while i <= 8 :
			start = self.connection.connection_params.data_area[i].start
			end = self.connection.connection_params.data_area[i].end
			dbNumber = self.connection.connection_params.data_area[i].db
			data_area = i
				
			if self.connection.connection_params.data_area[i].data_type == 3 and presetAddress >= start and presetAddress <= end : # coil data Type
				found = True
				break
			i = i + 1				
		if found == False :
			raise AwlSimError("Holding Register Memory not defined. Cannot preset holding registers")
		
		register_word = presetAddress - start
		if type == 3 :
			#register_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Holding_Register_" + str(register_word))
			register_word_data = self.cpu.dbs[dbNumber].structInstance.getFieldDataByName("Holding_Register_" + str(register_word))
		if type == 4 :
			register_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Input_Register_" + str(register_word))
		return register_word_data
		

	def load_msg_params(self,params) :
		
		msgSum = self.slaveAddress + self.functionCode + self.transaction_id

		if self.functionCode in [1,2,3,4] :	# Read Coils, Inputs, Holding Register or Input Register
			self.startingAddressHi = params["startingAddress"] >> 8
			msgSum = msgSum + self.startingAddressHi
			self.startingAddressLo = params["startingAddress"] & 0x00FF
			msgSum = msgSum + self.startingAddressLo
			self.numberOfPointsHi = params["numberOfPoints"] >> 8
			msgSum = msgSum + self.numberOfPointsHi
			self.numberOfPointsLo = params["numberOfPoints"] & 0x00FF
			msgSum = msgSum + self.numberOfPointsLo
			
		elif self.functionCode == 5 :		# Preset Single Coil
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo			
			coil_status = self.read_coil_status(params["presetAddress"])
			
			if coil_status != 0 :
				self.presetDataHi = 0xFF
				self.presetDataLo = 0x00
			else :
				self.presetDataHi = 0x00
				self.presetDataLo = 0x00

			msgSum = msgSum + self.presetDataHi + self.presetDataLo

		elif self.functionCode == 6 : 		# Preset Single Register
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo
			
			register_word_data  = self.read_holding_register(params["presetAddress"])
			print("Register word data = ",register_word_data)
			self.presetDataHi = register_word_data >> 8 
			self.presetDataLo = register_word_data & 0x00FF

			msgSum = msgSum + self.presetDataHi + self.presetDataLo

		elif self.functionCode == 15 : 		# Preset Multiple Coils
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo
			self.numberPresetHi = params["numberPreset"] >> 8
			msgSum = msgSum + self.numberPresetHi
			self.numberPresetLo = params["numberPreset"] & 0x00FF
			msgSum = msgSum + self.numberPresetLo

			self.presetDataHi = []
			self.presetDataLo = []
			
			byteOffset = 0
			bitOffset = 0
			n_coils = params["numberPreset"]
			currcoilAddress = params["presetAddress"]
			currcoilWord = 0
			while currcoilAddress < params["presetAddress"] + n_coils :
				curr_coil_status  = self.read_coil_status(currcoilAddress)
				currcoilWord = currcoilWord | ( curr_coil_status << bitOffset)
				bitOffset = bitOffset + 1
				if bitOffset == 8 :
					bitOffset = 0
					if byteOffset % 2 == 0 :
						self.presetDataHi.append(currcoilWord & 0xFF)
					else :
						self.presetDataLo.append(currcoilWord & 0xFF)
					byteOffset = byteOffset + 1
					currcoilWord = 0
				currcoilAddress = currcoilAddress + 1
			if bitOffset > 0 :
				if byteOffset % 2 == 0 :
					self.presetDataHi.append(currcoilWord)
				else :
					self.presetDataLo.append(currcoilWord)
				
			self.byteCount = len(self.presetDataHi) + len(self.presetDataLo)
			msgSum = msgSum + self.byteCount + sum(self.presetDataHi) + sum(self.presetDataLo)

		elif self.functionCode == 16 :
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo
			self.numberPresetHi = params["numberPreset"] >> 8
			msgSum = msgSum + self.numberPresetHi
			self.numberPresetLo = params["numberPreset"] & 0x00FF
			msgSum = msgSum + self.numberPresetLo

			self.presetDataHi = []
			self.presetDataLo = []
			
			n_regs = params["numberPreset"]
			currregAddress = params["presetAddress"]
			while currregAddress < params["presetAddress"] + n_regs :
				curr_reg_status  = self.read_holding_register(currregAddress)
				self.presetDataHi.append(curr_reg_status >> 8)
				self.presetDataLo.append(curr_reg_status & 0x00FF)
				currregAddress = currregAddress + 1
				
			self.byteCount = len(self.presetDataHi) + len(self.presetDataLo)
			msgSum = msgSum + self.byteCount + sum(self.presetDataHi) + sum(self.presetDataLo)

		self.msgCRC = msgSum & 0xFFFF
		self.msgCRC = self.msgCRC%256

	def construct_request_message(self,params) :
		
		self.load_msg_params(params)
		self.msg = bytearray()
		self.msg.append(self.slaveAddress)
		self.msg.append(self.transaction_id)
		self.msg.append(self.functionCode)
		if self.functionCode in [1,2,3,4] :
			self.msg.append(self.startingAddressHi)
			self.msg.append(self.startingAddressLo)
			self.msg.append(self.numberOfPointsHi)
			self.msg.append(self.numberOfPointsLo)
			self.msg.append(self.msgCRC)
		elif self.functionCode == 5 or self.functionCode == 6 :
			self.msg.append(self.presetAddressHi)
			self.msg.append(self.presetAddressLo)
			self.msg.append(self.presetDataHi)
			self.msg.append(self.presetDataLo)
			self.msg.append(self.msgCRC)
		else :
			self.msg.append(self.presetAddressHi)
			self.msg.append(self.presetAddressLo)
			self.msg.append(self.numberPresetHi)
			self.msg.append(self.numberPresetLo)
			self.msg.append(self.byteCount)
			i = 0
			while i < self.byteCount :
				if i % 2 == 0 :
					self.msg.append(self.presetDataHi[int(i/2)])
				else :
					self.msg.append(self.presetDataLo[int(i/2)])
				i = i + 1
			self.msg.append(self.msgCRC)

		return self.msg
			



class ModBusResponseMessage(object) :




	def __init__(self,connection,functionCode,slaveAddress,t_id) :
		self.slaveAddress = slaveAddress
		self.functionCode = functionCode
		self.transaction_id = t_id
		self.byteCount = 0	
		self.msgCRC = 0
		self.connection = connection
		self.ERROR_CODE = NO_ERROR

	def read_bit_status(self,type,presetAddress) :
		i = 1
		found = False
		while i <= 8 :
			start = self.connection.connection_params.data_area[i].start
			end = self.connection.connection_params.data_area[i].end
			dbNumber = self.connection.connection_params.data_area[i].db
			data_area = i
				
			if self.connection.connection_params.data_area[i].data_type == type and presetAddress >= start and presetAddress <= end : # coil data Type
				found = True
				break
			i = i + 1				
		if found == False :
			self.ERROR_CODE  = ILLEGAL_DATA_ADDRESS
			return -1

		word = (presetAddress - start)/16 
		bit_in_word = (presetAddress - start)%16
		if type == 1 : 
			word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Coils_" +  str(word))
		if type == 2 :
			word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Inputs_" +  str(word))

		status = (word_data & (1 << bit_in_word)) >> bit_in_word
		return status

	
	def read_register(self,type,presetAddress) :
		i = 1
		found = False
		while i <= 8 :
			start = self.connection.connection_params.data_area[i].start
			end = self.connection.connection_params.data_area[i].end
			dbNumber = self.connection.connection_params.data_area[i].db
			data_area = i
				
			if self.connection.connection_params.data_area[i].data_type == type and presetAddress >= start and presetAddress <= end : # coil data Type
				found = True
				break
			i = i + 1				
		if found == False :
			self.ERROR_CODE = ILLEGAL_DATA_ADDRESS
			return -1
		
		register_word = presetAddress - start
		if type == 3 : 
			register_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Holding_Register_" + str(register_word))
		if type == 4 :
			register_word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Input_Register_" + str(register_word))
		return register_word_data
	

	def load_msg_params(self,params) :
		
		msgSum = self.slaveAddress + self.functionCode + self.transaction_id

		if self.functionCode not in [1,2,3,4,5,6,15,16] :
			self.ERROR_CODE = ILLEGAL_FUNCTION
			return

		if self.functionCode == 1 or self.functionCode == 2 :
			self.readDataHi = []
			self.readDataLo = []
			
			byteOffset = 0
			bitOffset = 0
			n_reads = params["numberOfPoints"]
			currAddress = params["startingAddress"]
			currlWord = 0
			while currAddress < params["startingAddress"] + n_reads :
				curr_status  = self.read_bit_status(self.functionCode,currAddress)
				if self.ERROR_CODE != NO_ERROR :
					return
				currWord = currWord | ( curr_status << bitOffset)
				bitOffset = bitOffset + 1
				if bitOffset == 8 :
					bitOffset = 0
					if byteOffset % 2 == 0 :
						self.readDataHi.append(currWord & 0xFF)
					else :
						self.readDataLo.append(currWord & 0xFF)
					byteOffset = byteOffset + 1
					currWord = 0
				currAddress = currAddress + 1
			if bitOffset > 0 :
				if byteOffset % 2 == 0 :
					self.readDataHi.append(currWord)
				else :
					self.readDataLo.append(currWord)
				
			self.byteCount = len(self.readDataHi) + len(self.readDataLo)
			msgSum = msgSum + self.byteCount + sum(self.readDataHi) + sum(self.readDataLo)

		elif self.functionCode == 3 or self.functionCode == 4 :

			self.readDataHi = []
			self.readDataLo = []
			

			n_reads = params["numberOfPoints"]
			currAddress = params["startingAddress"]
			currlWord = 0
			while currAddress < params["startingAddress"] + n_reads :
				curr_status  = self.read_register(self.functionCode,currAddress)
				if self.ERROR_CODE != NO_ERROR :
					return

				self.readDataHi.append(curr_status >> 8)
				self.readDataLo.append(curr_status & 0xFF)
				currAddress = currAddress + 1


			self.byteCount = len(self.readDataHi) + len(self.readDataLo)
			msgSum = msgSum + self.byteCount + sum(self.readDataHi) + sum(self.readDataLo)

		elif self.functionCode == 5 :
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo

			self.readDataHi = 0
			self.readDataLo = 0
			
			presetAddress = params["presetAddress"]
			curr_status = self.read_bit_status(1,presetAddress)
			if self.ERROR_CODE != NO_ERROR :
				return

			if curr_status != 0 :
				self.readDataHi = 0xFF
				self.readDataLo = 0x00
			msgSum = msgSum + self.readDataHi + self.readDataLo

		elif self.functionCode == 6 :
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo

			self.readDataHi = 0
			self.readDataLo = 0
			
			presetAddress = params["presetAddress"]
			curr_status = self.read_register(3,presetAddress)
			if self.ERROR_CODE != NO_ERROR :
				return

			if curr_status != 0 :
				self.readDataHi = curr_status >> 8
				self.readDataLo = curr_status & 0x00FF

			msgSum = msgSum + self.readDataHi + self.readDataLo

		else :
			self.presetAddressHi = params["presetAddress"] >> 8
			msgSum = msgSum + self.presetAddressHi
			self.presetAddressLo = params["presetAddress"] & 0x00FF
			msgSum = msgSum + self.presetAddressLo
			self.numberPresetHi = params["numberPreset"] >> 8
			msgSum = msgSum + self.numberPresetHi
			self.numberPresetLo = params["numberPreset"] & 0x00FF
			msgSum = msgSum + self.numberPresetLo

		self.msgCRC = msgSum % 256

	def construct_response_message(self,params) :

		self.load_msg_params(params)
		self.msg = bytearray()
		self.msg.append(self.slaveAddress)
		self.msg.append(self.transaction_id)


		if self.ERROR_CODE != NO_ERROR :
			self.msg.append(0x81)
			self.msg.append(self.ERROR_CODE)
			self.msgCRC = (self.slaveAddress + 0x81 + self.ERROR_CODE) % 256
			self.msg.append(self.msgCRC)
			return self.msg

		self.msg.append(self.functionCode)
		if self.functionCode in [1,2,3,4] :
			self.msg.append(self.byteCount)
			i = 0
			while i < self.byteCount :
				if i % 2 == 0 :
					self.msg.append(self.readDataHi[int(i/2)])
				else :
					self.msg.append(self.readDataLo[int(i/2)])
				i = i + 1
			self.msg.append(self.msgCRC)

		elif self.functionCode == 5 or self.functionCode == 6 :
			self.msg.append(self.presetAddressHi)
			self.msg.append(self.presetAddressLo)
			self.msg.append(self.readDataHi)
			self.msg.append(self.readDataLo)
			self.msg.append(self.msgCRC)
		else :
			self.msg.append(self.presetAddressHi)
			self.msg.append(self.presetAddressLo)
			self.msg.append(self.numberPresetHi)
			self.msg.append(self.numberPresetLo)
			self.msg.append(self.msgCRC)

		return self.msg


class ModBusErrorMessage(object) :

	def __init__(self,slaveAddress,t_id,error_code) :
		self.error_code = error_code
		self.slaveAddress = slaveAddress
		self.transaction_id = t_id

	def get_error_message(self) :
		msg = bytearray()
		msg.append(self.slaveAddress)
		msg.append(self.transaction_id)
		msg.append(0x81)
		msg.append(self.error_code)
		CRC = (self.slaveAddress + 0x81 + self.error_code + self.transaction_id) % 256
		msg.append(CRC)
		return msg

			
