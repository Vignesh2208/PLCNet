from awlsim.core.systemblocks.modbus_message import *
from awlsim.core.systemblocks.exceptions import *
from awlsim.common.compat import *

from awlsim.core.systemblocks.systemblocks import *
from awlsim.core.util import *

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



	def __init__(self,connection) :
		self.connection = connection
		self.ERROR_CODE = NO_ERROR
		pass


	def write_bit_status(self,presetAddress,bitvalue,type) :
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
			self.ERROR_CODE  = ERROR_INVALID_COMBINATION
			return

		word = (presetAddress - start)/16 
		bit_in_word = (presetAddress - start)%16
		
		if type == 1 :
			word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Coils_" +  str(word))
			if bitvalue > 0 :
				status = (word_data | (bitvalue << bit_in_word))
			else :
				status = (word_data & ~(1 << bit_in_word))

			self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Coils_" + str(word), status)
		if type == 2 :
			word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Inputs_" +  str(word))
			if bitvalue > 0 :
				status = (word_data | (bitvalue << bit_in_word))
			else :
				status = (word_data & ~(1 << bit_in_word))
			
			self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Inputs_" + str(word), status)


	def write_reg_status(self,presetAddress,regvalue,type) :
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
			self.ERROR_CODE  = ERROR_INVALID_COMBINATION
			return

		word = presetAddress - start
		if type == 3 :
			self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Holding_Register_" + str(word), regvalue)
		if type == 4 :
			self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Input_Register_" + str(word), regvalue)

	def construct_request_message(self,dataType,write_read,length,single_write,startAddress,transaction_id,slave_add) :

		if dataType > 4 or dataType < 0 :
			#raise AwlSimError("Invalid dataType Error Code 0xA011 : ", dataType)
			print("Invalid dataType Error Code 0xA011 : ", dataType)
			return None,ERROR_INVALID_DATATYPE
		if length < 0 :
			#raise AwlSimError("Length cannot be less than zero. Error Code : 0xA005")
			print("Length cannot be less than zero. Error Code : 0xA005")
			return None,ERROR_INVALID_LENGTH

		slaveAddress = slave_add
		t_id = transaction_id

		if write_read == 0 : 	# reading function
			functionCode = dataType
			if length > max_dataType_reading_length[dataType] :
				#raise AwlSimError("Reading length exceeds the limit. Error Code : 0xA005")
				print("Reading length exceeds the limit. Error Code : 0xA005")
				return None,ERROR_INVALID_LENGTH
		else :
			if dataType == 2 or dataType == 4 :
				#raise AwlSimError("Cannot write to inputs or input registers. Error Code : 0xA004 ")
				print("Cannot write to inputs or input registers. Error Code : 0xA004 ")
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
				#raise AwlSimError("Writing length exceeds the limit. Error Code : 0xA005")
				print("Writing length exceeds the limit. Error Code : 0xA005")
				return None,ERROR_INVALID_LENGTH
		request_msg_constructer = ModBusRequestMessage(connection=self.connection,slaveAddress=slaveAddress,functionCode=functionCode,t_id=t_id)
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

		#print("FunctionCode = ",functionCode)
		

		request_msg = request_msg_constructer.construct_request_message(msg_params)
		self.sent_msg = request_msg
		
		return request_msg,NO_ERROR

	def process_response_message(self,msg) :
		
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

		

