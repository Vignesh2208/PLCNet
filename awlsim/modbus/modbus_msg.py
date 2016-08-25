

ERROR_INCORRECT_CALL_PARAMS = 0xA003
ERROR_INVALID_WRITE_ACTION = 0xA004
ERROR_INVALID_LENGTH = 0xA005
ERROR_INVALID_COMBINATION = 0xA006
ERROR_INVALID_MONITORING_TIME = 0xA007
ERROR_INVALID_TI = 0xA009
ERROR_INVALID_UNIT = 0xA00A
ERROR_INVALID_FUNCTION_CODE = 0xA00B
ERROR_INVALID_REG_BIT_COUNT = 0xA00D
ERROR_INVALID_DATATYPE = 0xA011
ERROR_INVALID_MSG_ECHO_FN5 = 0xA081
ERROR_INVALID_MSG_ECHO_FN6 = 0xA082
ERROR_UNKNOWN_EXCEPTION = 0xA095
ERROR_BUSY = 0xA083
ERROR_MONITORING_TIME_ELAPSED = 0xA100



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


	def __init__(self,functionCode,slaveAddress,t_id,coil_mem,reg_mem) :
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
		self.coil_mem = coil_mem
		self.reg_mem = reg_mem

	def read_coil_status(self,coil_Address) :
		
		addr = "Coils_" + str(coil_Address)
		if addr not in self.coil_mem.keys() :
			return 0

		return self.coil_mem[addr]
	
	def read_holding_register(self,reg_Address) :
		
		addr = "Holding_Register_" + str(reg_Address)
		if addr not in self.reg_mem.keys() :
			return 0

		return self.reg_mem[addr]
		

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