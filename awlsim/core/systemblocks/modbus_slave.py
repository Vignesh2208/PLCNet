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

class ModBusSlave(object) :




	def __init__(self,connection) :
		self.connection = connection
		self.ERROR_CODE = NO_ERROR
		pass

	def write_bit_status(self,presetAddress,bitvalue) :
		i = 1
		found = False
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
			self.ERROR_CODE  = ERROR_INVALID_COMBINATION
			return

		word = (presetAddress - start)/16 
		bit_in_word = (presetAddress - start)%16
		
		word_data = self.connection.data_area_dbs[data_area].structInstance.getFieldDataByName("Coils_" +  str(word))
		if bitvalue > 0 :
			status = (word_data | (bitvalue << bit_in_word))
		else :
			status = (word_data & ~(1 << bit_in_word))
				
		self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Coils_" + str(word), status)


	def write_reg_status(self,presetAddress,regvalue) :
		i = 1
		found = False
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
			self.ERROR_CODE  = ERROR_INVALID_COMBINATION
			return

		word = presetAddress - start
		self.connection.data_area_dbs[data_area].structInstance.setFieldDataByName("Holding_Register_" + str(word), regvalue)

	# also constructs response message and returns it
	def process_request_message(self,msg) :
		request_msg_params = {}
		request_msg_params["unit"] = -1
		request_msg_params["ti"] = msg[1]
		request_msg_params["data_type"] = -1
		request_msg_params["write_read"] = -1
		request_msg_params["start_address"] =  -1
		request_msg_params["length"] =  -1
		request_msg_params["ERROR"] = NO_ERROR

		try :
			# msg is expected to be a byte array
			slaveAddress = msg[0]
			functionCode = msg[1 + 1]
			ti = msg[1]
			if functionCode not in [1,2,3,4,5,6,15,16] :
				error_msg_construct = ModBusErrorMessage(slaveAddress,ti,ILLEGAL_FUNCTION)
				request_msg_params["ERROR"] = ILLEGAL_FUNCTION
				return error_msg_construct.get_error_message(),request_msg_params

			response_msg_construct = ModBusResponseMessage(connection=self.connection,slaveAddress=slaveAddress,functionCode=functionCode,t_id = ti)
			response_msg_params = {}
			
			if functionCode == 1 or functionCode == 5 or functionCode == 15 :
				request_msg_params["data_type"] = 1								
			if functionCode == 3 or functionCode == 6 or functionCode == 16 :						
				request_msg_params["data_type"] = 3
			else :
				request_msg_params["data_type"] = functionCode
			

			if functionCode in [1,2,3,4] :
				startingAddressHi = msg[2+1]
				startingAddressLo = msg[3+1]
				numberOfPointsHi = msg[4+1]
				numberOfPointsLo = msg[5+1]

				startingAddress = (startingAddressHi << 8) | startingAddressLo				
				numberOfPoints = (numberOfPointsHi << 8) | numberOfPointsLo
				response_msg_params["startingAddress"] = startingAddress
				response_msg_params["numberOfPoints"] = numberOfPoints

				request_msg_params["write_read"] = 0
				request_msg_params["start_address"] =  startingAddress
				request_msg_params["length"] =  numberOfPoints

				if numberOfPoints > max_dataType_reading_length[functionCode] :
					print("Reading length exceeds the limit. Error Code : 0xA005")
					request_msg_params["ERROR"] = ERROR_INVALID_LENGTH
					error_msg_construct = ModBusErrorMessage(slaveAddress,ti,ERROR_INVALID_LENGTH)
					return error_msg_construct.get_error_message(),request_msg_params
				
				return response_msg_construct.construct_response_message(response_msg_params),request_msg_params

			if functionCode == 5 :
				presetAddressHi = msg[2+1]
				presetAddressLo = msg[3+1]
				presetDataHi = msg[4+1]
				presetDataLo = msg[5+1]

				presetAddress = (presetAddressHi << 8) | presetAddressLo


				request_msg_params["write_read"] = 1
				request_msg_params["start_address"] =  presetAddress
				request_msg_params["length"] =  1

				coil_preset_value = 1 if presetDataHi == 0xFF else 0
				self.write_bit_status(presetAddress,coil_preset_value)
				if self.ERROR_CODE != NO_ERROR :
					error_msg_construct = ModBusErrorMessage(slaveAddress,ti,self.ERROR_CODE)
					request_msg_params["ERROR"] = ERROR_INVALID_COMBINATION
					return error_msg_construct.get_error_message()

				response_msg_params["presetAddress"] = presetAddress
				return response_msg_construct.construct_response_message(response_msg_params),request_msg_params

							
			if functionCode == 6 :
				presetAddressHi = msg[2+1]
				presetAddressLo = msg[3+1]
				presetDataHi = msg[4+1]
				presetDataLo = msg[5+1]

				presetAddress = (presetAddressHi << 8) | presetAddressLo
				presetData = (presetDataHi << 8 ) | presetDataLo

				request_msg_params["write_read"] = 1
				request_msg_params["start_address"] =  presetAddress
				request_msg_params["length"] =  1

				self.write_reg_status(presetAddress,presetData)
				if self.ERROR_CODE != NO_ERROR :
					error_msg_construct = ModBusErrorMessage(slaveAddress,ti,self.ERROR_CODE)
					request_msg_params["ERROR"] = ERROR_INVALID_COMBINATION
					return error_msg_construct.get_error_message(),request_msg_params

				response_msg_params["presetAddress"] = presetAddress
				return response_msg_construct.construct_response_message(response_msg_params),request_msg_params

			if functionCode == 15 :
				presetAddressHi = msg[2+1]
				presetAddressLo = msg[3+1]
				numberPresetHi = msg[4+1]
				numberPresetLo = msg[5+1]

				presetAddress = (presetAddressHi << 8) | presetAddressLo
				numberPreset = (numberPresetHi << 8 ) | numberPresetLo

				request_msg_params["write_read"] = 1
				request_msg_params["start_address"] =  presetAddress
				request_msg_params["length"] =  numberPreset

				if numberPreset > max_dataType_writing_length[1] :
					print("Writing length exceeds the limit. Error Code : 0xA005")
					request_msg_params["ERROR"] = ERROR_INVALID_LENGTH
					error_msg_construct = ModBusErrorMessage(slaveAddress,ti,ERROR_INVALID_LENGTH)
					return error_msg_construct.get_error_message(),request_msg_params

				byteCount = msg[6+1]
				byteOffset = 0
				bitOffset = 0
				currcoilAddress = presetAddress
				while currcoilAddress < presetAddress + numberPreset :
					curr_coil_status  = 1 if (msg[7 + 1 + byteOffset] & (1 << bitOffset)) != 0 else 0
					self.write_bit_status(currcoilAddress,curr_coil_status)
					if self.ERROR_CODE != NO_ERROR :
						error_msg_construct = ModBusErrorMessage(slaveAddress,ti,self.ERROR_CODE)
						request_msg_params["ERROR"] = ERROR_INVALID_COMBINATION
						return error_msg_construct.get_error_message(),request_msg_params
					
					bitOffset = bitOffset + 1
					if bitOffset == 8 :
						bitOffset = 0
						byteOffset = byteOffset + 1
					currcoilAddress = currcoilAddress + 1
				response_msg_params["presetAddress"] = presetAddress
				response_msg_params["numberPreset"] = numberPreset
				return response_msg_construct.construct_response_message(response_msg_params),request_msg_params
				

			if functionCode == 16 : 
				presetAddressHi = msg[2+1]
				presetAddressLo = msg[3+1]
				numberPresetHi = msg[4+1]
				numberPresetLo = msg[5+1]

				presetAddress = (presetAddressHi << 8) | presetAddressLo
				numberPreset = (numberPresetHi << 8 ) | numberPresetLo

				request_msg_params["write_read"] = 1
				request_msg_params["start_address"] =  presetAddress
				request_msg_params["length"] =  numberPreset
				
				if numberPreset > max_dataType_writing_length[3] :
					print("Writing length exceeds the limit. Error Code : 0xA005")
					request_msg_params["ERROR"] = ERROR_INVALID_LENGTH
					error_msg_construct = ModBusErrorMessage(slaveAddress,ti,ERROR_INVALID_LENGTH)
					return error_msg_construct.get_error_message(),request_msg_params

				byteCount = msg[6+1]
				byteOffset = 0
				currregAddress = presetAddress
				while currregAddress < presetAddress + numberPreset :
					curr_reg_status  = (msg[7 +1 + byteOffset] << 8) | msg[7 + 1 + byteOffset + 1]
					self.write_reg_status(currregAddress,curr_reg_status)
					if self.ERROR_CODE != NO_ERROR :
						error_msg_construct = ModBusErrorMessage(slaveAddress,ti,self.ERROR_CODE)
						request_msg_params["ERROR"] = ERROR_INVALID_COMBINATION
						return error_msg_construct.get_error_message(),request_msg_params
					
					byteOffset = byteOffset + 2
					currregAddress = currregAddress + 1

				response_msg_params["presetAddress"] = presetAddress
				response_msg_params["numberPreset"] = numberPreset
				return response_msg_construct.construct_response_message(response_msg_params),request_msg_params						

				

		except IndexError:
			print("WARNING ! MSG in improper format")
			error_msg_construct = ModBusErrorMessage(msg[0],ti,ERROR_UNKNOWN_EXCEPTION)
			request_msg_params["ERROR"] = ERROR_UNKNOWN_EXCEPTION
			return error_msg_construct.get_error_message(),request_msg_params

