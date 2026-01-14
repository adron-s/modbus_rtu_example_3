import struct
from typing import TypeAlias
from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException
from pymodbus import pymodbus_apply_logging_config
from pymodbus.pdu.file_message import FileRecord, ReadFileRecordRequest

pymodbus_apply_logging_config("DEBUG")

MOBDUS_DEVICE_ID = 0x01
MOBDUS_PAYLOAD_CHUNK_SIZE = 240
MOBDUS_FILE_SEND_HEAD_REC_NUM = 9998
MOBDUS_FILE_SEND_TAIL_REC_NUM = 9999

ChunkList: TypeAlias = list[tuple[int, bytes]]

# Configure the Modbus RTU Client.
client = ModbusSerialClient(
	port='/dev/ttyUSB2',
	baudrate=115200,
	parity='N',
	stopbits=1,
	bytesize=8,
	timeout=0.4, # seconds to wait for a response
	retries=5
)

def send_data_chunks_via_modbus(data: ChunkList) -> None:
	"""
	Performs transmission of pre-cooked binary data chunks using
	modbus write file record (FC = 21).
	"""
	# Attempt to connect to the serial port.
	if not client.connect():
		print("Failed to connect to the serial port")
		return

	try:
		if len(data) > MOBDUS_FILE_SEND_TAIL_REC_NUM:
			raise ValueError("data length is to big to bit into MODBUS regs!")

		# An arbitrary number indicating the number of the file we are working with.
		file_number = 10

		for record_num, record_data in data:
			# Wrap chunks with data in the required format.
			records = [
				FileRecord(
					file_number=file_number,
					record_number=record_num,
					record_data=record_data
				)
			]
			# And send the current chunk to the slave.
			result = client.write_file_record(
				records=records, device_id=MOBDUS_DEVICE_ID
			)

			# Check for errors in the response.
			if result.isError():
				print(f"Modbus Error: {result}")
				break
			else:
				# result.registers contains the list of values.
				print(f"Success! Data: {result.registers}")

	except ModbusException as e:
		print(f"Communication exception: {e}")
	finally:
		# Always close the connection
		client.close()

def read_file_and_prep_data_list() -> ChunkList:
	"""
	Splits a text (json) file into binary chunks.
	"""
	result: list[bytes] = [ ]

	with open("./request.json", "rb") as f:
		data = f.read()
		total_len = len(data)
		data = [data[i:i+MOBDUS_PAYLOAD_CHUNK_SIZE]
			for i in range(0, len(data), MOBDUS_PAYLOAD_CHUNK_SIZE)]

		n_chunks = len(data)

		if len(data) > MOBDUS_FILE_SEND_HEAD_REC_NUM:
			raise ValueError("data length is to big to fit into MODBUS regs!")

		head = struct.pack("<II", total_len, n_chunks)
		print(f"{total_len=}, {n_chunks=}, {head=}")

		result = [
			(MOBDUS_FILE_SEND_HEAD_REC_NUM, head),
			*enumerate(data),
			(MOBDUS_FILE_SEND_TAIL_REC_NUM, b'')
		]

		return result

if __name__ == "__main__":
	data = read_file_and_prep_data_list()
	send_data_chunks_via_modbus(data)
