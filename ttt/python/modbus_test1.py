import time
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
		device_id = 0x01
		led_state = 0
		counter = None
		# Read Holding Registers (Function Code 0x03)
		# Parameters: address, count, slave (device ID)
		while True:
			for stage in range(2):
				if stage == 0:
					if counter is None:
						continue

					result = client.write_registers(address=0, values=[counter, led_state], device_id=device_id)
					led_state = int(not led_state)
					counter += 1
				elif stage == 1:
					result = client.read_holding_registers(address=0, count=2, device_id=device_id)

				# Check for errors in the response
				if result.isError():
					print(f"Modbus Error: {result}")
				else:
					# result.registers contains the list of values
					print(f"Success! Data: {result.registers}")
					if counter is None and stage == 1 and result.registers:
						counter = result.registers[0]

			print("*" * 50)
			time.sleep(1)

			# if counter is not None and led_state == 1:
			# 	break

	except ModbusException as e:
		print(f"Communication exception: {e}")
	finally:
		# Always close the connection
		client.close()

if __name__ == "__main__":
	read_rs485_data()
