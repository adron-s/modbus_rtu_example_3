from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException
from pymodbus import pymodbus_apply_logging_config

pymodbus_apply_logging_config("DEBUG")

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

def read_rs485_data():
	# Attempt to connect to the serial port.
	if not client.connect():
		print("Failed to connect to the serial port")
		return

	try:
		# Read Holding Registers (Function Code 0x03)
		# Parameters: address, count, slave (device ID)
		result = client.read_holding_registers(address=0, count=1, device_id=0x1)

		# Check for errors in the response
		if result.isError():
			print(f"Modbus Error: {result}")
		else:
			# result.registers contains the list of values
			print(f"Success! Data: {result.registers}")

	except ModbusException as e:
		print(f"Communication exception: {e}")
	finally:
		# Always close the connection
		client.close()

if __name__ == "__main__":
	read_rs485_data()
