from modbus_msg import *

max_dataType_reading_length = {
	1 : 2000,
	2 : 2000,
	3 : 125,
	4 : 125,
}

max_dataType_writing_length = {

	1 : 800,
	2 : -1,
	3 : 100,
	4 : -1,
}

NO_ERROR = 0
ILLEGAL_FUNCTION = 1
ILLEGAL_DATA_ADDRESS = 2
ILLEGAL_DATA_VALUE = 3
SLAVE_DEVICE_FAILURE = 4


class ModBusMaster(object) :



	def __init__(self,coil_mem,reg_mem) :
		self.coil_mem = coil_mem
		self.reg_mem = reg_mem
		self.ERROR_CODE = NO_ERROR
		pass


	def write_bit_status(self,presetAddress,bitvalue,type) :		
		
		if type == 1 :
			addr = "Coils_" + str(presetAddress)
		
		if type == 2 :
			addr = "Inputs_" + str(presetAddress)

		self.coil_mem[addr] = bitvalue			


	def write_reg_status(self,presetAddress,regvalue,type) :
		
		if type == 3 :
			addr = "Holding_Register_" + str(presetAddress)
			
		if type == 4 :
			addr = "Input_Register_" + str(presetAddress)

		self.reg_mem[addr] = regvalue

	def construct_request(self,mem_type,mem_access_type,start_addr,n_accesses,transaction_id,slave_addr) :
		if mem_type == "COIL" :
			datatype = 1

		elif mem_type == "INPUT" :
			datatype = 2

		elif mem_type == "HOLDING_REG" :
			datatype = 3

		elif mem_type == "INPUT_REG" :
			datatype = 4

		else :
			return None,ERROR_INVALID_DATATYPE

		if mem_access_type == "READ" :
			write_read = 0
		elif mem_access_type == "WRITE" :
			write_read = 1
		else :
			return None,ERROR_INVALID_WRITE_ACTION

		length = int(n_accesses)
		if length == 1 :
			single_write = 1
		else :
			single_write = 0

		return self.construct_request_message(datatype,write_read,length,single_write,start_addr,transaction_id,slave_addr)


	def construct_request_message(self,dataType,write_read,length,single_write,startAddress,transaction_id,slave_add) :

		if dataType > 4 or dataType < 0 :
			print("Invalid dataType Error Code 0xA011 : ", dataType)
			return None,ERROR_INVALID_DATATYPE
		if length < 0 :
			print("Length cannot be less than zero. Error Code : 0xA005")
			return None,ERROR_INVALID_LENGTH

		slaveAddress = slave_add
		t_id = transaction_id

		if write_read == 0 : 	# reading function
			functionCode = dataType
			if length > max_dataType_reading_length[dataType] :
				print("Reading length exceeds the limit. Error Code : 0xA005")
				return None,ERROR_INVALID_LENGTH
		else :
			if dataType == 2 or dataType == 4 :
				print("Cannot write to inputs or input registers of PLC. Not supported. Error Code : 0xA004 ")
				return None,ERROR_INVALID_WRITE_ACTION

			
			if length > 1 and dataType == 1:
				functionCode = 15
			if length > 1 and dataType == 3 :
				functionCode = 16
			if dataType == 1 and length == 1 and single_write == 1 :
				functionCode = 5
			if dataType == 1 and length == 1 and single_write == 0 :
				functionCode = 15
		
			if dataType == 3 and length == 1 and single_write == 1 :
				functionCode = 6
			if dataType == 3 and length == 1 and single_write == 0 :
				functionCode = 16	

			if length > max_dataType_writing_length[dataType] :
				print("Writing length exceeds the limit. Error Code : 0xA005")
				return None,ERROR_INVALID_LENGTH

		request_msg_constructer = ModBusRequestMessage(coil_mem=self.coil_mem,reg_mem=self.reg_mem,slaveAddress=slaveAddress,functionCode=functionCode,t_id=t_id)
		msg_params = {}
		if functionCode in [1,2,3,4] :
			msg_params["startingAddress"] = startAddress
			msg_params["numberOfPoints"] = length
		elif functionCode == 5 or functionCode == 6 :
			msg_params["presetAddress"] = startAddress
		else :
			msg_params["presetAddress"] = startAddress
			msg_params["numberPreset"] = length
	
		self.sentstartAddress = startAddress
		self.sentlength = length
		self.sentfunctionCode = functionCode
		self.sentTI = transaction_id
		self.sentslaveAddr = slaveAddress
		request_msg = request_msg_constructer.construct_request_message(msg_params)
		self.sent_msg = request_msg
		
		return request_msg,NO_ERROR

	def process_response(self,msg) :
		
		if len(msg) <= 1 :
			return ERROR_UNKNOWN_EXCEPTION 	# unknown exception error Code
		functionCode = int(msg[2])
		#print("Function Code = ",functionCode)
		ti = msg[1]
		recv_slave_add = msg[0]
		if ti == self.sentTI and functionCode != self.sentfunctionCode :
			return ERROR_INVALID_FUNCTION_CODE

		if ti  != self.sentTI or recv_slave_add != self.sentslaveAddr :
			return ERROR_INVALID_TI

		if functionCode == 0x81:
			if msg[3] == ILLEGAL_DATA_ADDRESS :
				return ERROR_INVALID_COMBINATION

			if msg[3] == ILLEGAL_DATA_VALUE or msg[2] == SLAVE_DEVICE_FAILURE:
				return ERROR_UNKNOWN_EXCEPTION

			else :
				return msg[3]				
		if functionCode == 5 or functionCode == 6 :			
			if self.sent_msg != msg :
				print("Sent message not echoed back for functionCode 5 /6 ")
				return (ERROR_INVALID_MSG_ECHO_FN5 if functionCode == 5 else ERROR_INVALID_MSG_ECHO_FN6)

		if functionCode not in [1,2,3,4,5,6,15,16] :
			return ERROR_INVALID_FUNCTION_CODE

		# if functionCode in [1,2,3,4] - write the data read from slave into a local DB
		if functionCode in [1,2] :
			byteCount = int(msg[3])
			presetAddress = self.sentstartAddress
			numberPreset = self.sentlength

			if byteCount != numberPreset*2 :
				return ERROR_INVALID_REG_BIT_COUNT

			byteOffset = 0
			bitOffset = 0
			currcoilAddress = presetAddress
			while currcoilAddress < presetAddress + numberPreset :
				curr_coil_status  = 1 if (msg[4 + byteOffset] & (1 << bitOffset)) != 0 else 0
				self.write_bit_status(currcoilAddress,curr_coil_status,functionCode)
				if self.ERROR_CODE != NO_ERROR :					
					return  ERROR_INVALID_COMBINATION
					
				bitOffset = bitOffset + 1
				if bitOffset == 8 :
					bitOffset = 0
					byteOffset = byteOffset + 1
				currcoilAddress = currcoilAddress + 1

		if functionCode in [3,4] :
			byteCount = int(msg[3])
			presetAddress = self.sentstartAddress
			numberPreset = self.sentlength

			if byteCount != numberPreset*2 :
				return ERROR_INVALID_REG_BIT_COUNT

			byteOffset = 0
			currregAddress = presetAddress
			while currregAddress < presetAddress + numberPreset :
				curr_reg_status  = (msg[4 + byteOffset] << 8) | msg[4 + byteOffset + 1]
				self.write_reg_status(currregAddress,curr_reg_status,functionCode)
				if self.ERROR_CODE != NO_ERROR :
					return ERROR_INVALID_COMBINATION					
					
				byteOffset = byteOffset + 2
				currregAddress = currregAddress + 1		
			
			

		return NO_ERROR

		

