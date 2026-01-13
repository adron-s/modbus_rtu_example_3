#include "nmbs/port.h"
#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t modbus_file_send_data_recv_buf[MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE];
uint8_t *modbus_file_send_data_recv_buf_ptr = modbus_file_send_data_recv_buf;
uint32_t modbus_file_send_data_recv_buf_rest = 0;

static int32_t read_serial(uint8_t *buf, uint16_t count,
			   int32_t byte_timeout_ms, void *arg);
static int32_t write_serial(const uint8_t *buf, uint16_t count,
			    int32_t byte_timeout_ms, void *arg);

static nmbs_error server_read_holding_registers(uint16_t, uint16_t, uint16_t *,
						uint8_t, void *);

static nmbs_error server_write_single_register(uint16_t, uint16_t, uint8_t,
					       void *);
static nmbs_error server_write_multiple_registers(uint16_t, uint16_t,
						  const uint16_t *, uint8_t,
						  void *);
static nmbs_error server_write_file_record(uint16_t, uint16_t, const uint16_t *,
					   uint16_t, uint8_t, void *);

nmbs_error nmbs_server_init(nmbs_t *nmbs, const uint8_t nmbs_id)
{
	nmbs_platform_conf conf;
	nmbs_callbacks cb;

	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = read_serial;
	conf.write = write_serial;

	nmbs_callbacks_create(&cb);
	cb.read_holding_registers = server_read_holding_registers;
	cb.write_single_register = server_write_single_register;
	cb.write_multiple_registers = server_write_multiple_registers;
	cb.write_file_record = server_write_file_record;

	nmbs_error status = nmbs_server_create(nmbs, nmbs_id, &conf, &cb);
	if (status != NMBS_ERROR_NONE) {
		return status;
	}

	nmbs_set_byte_timeout(nmbs, 100);
	nmbs_set_read_timeout(nmbs, 200);

	return NMBS_ERROR_NONE;
}

static nmbs_error server_read_holding_registers(uint16_t address,
						uint16_t quantity,
						uint16_t *registers_out,
						uint8_t unit_id, void *arg)
{
	printf("%s, address: %d, unit_id: %d\n", __func__, address, unit_id);

	for (size_t i = 0; i < quantity; i++) {
		uint16_t cur_reg = address + i;

		switch (cur_reg) {
		case 0:
			registers_out[i] = loop_count;
			break;

		case 1:
			registers_out[i] = get_led_pin_state();
			break;

		default:
			return NMBS_ERROR_INVALID_REQUEST;
		}
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error server_write_single_register(uint16_t address, uint16_t value,
					       uint8_t unit_id, void *arg)
{
	uint16_t reg = value;
	return server_write_multiple_registers(address, 1, &reg, unit_id, arg);
}

static nmbs_error server_write_multiple_registers(uint16_t address,
						  uint16_t quantity,
						  const uint16_t *registers,
						  uint8_t unit_id, void *arg)
{
	printf("%s, address: %d, quantity: %d, unit_id: %d\n", __func__,
	       address, quantity, unit_id);
	for (size_t i = 0; i < quantity; i++) {
		uint16_t cur_reg = address + i;

		switch (cur_reg) {
		case 0:
			loop_count = registers[i];
			break;

		case 1:
			set_led_pin_state(registers[i]);
			break;

		default:
			return NMBS_ERROR_INVALID_REQUEST;
		}
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error server_write_file_record(uint16_t file_number,
					   uint16_t record_number,
					   const uint16_t *registers,
					   uint16_t count, uint8_t unit_id,
					   void *arg)
{
	printf(
	    "%s, file_number: %d, record_number: %d, count: %d, unit_id: %d\n",
	    __func__, file_number, record_number, count, unit_id);


	if (record_number > MOBDUS_FILE_SEND_TAIL_REC_NUM) {
		printf("ERROR: %s:: invalid record_number (%u) !!!\n", __func__, record_number);
		return NMBS_ERROR_INVALID_REQUEST;
	}

	if (record_number == MOBDUS_FILE_SEND_HEAD_REC_NUM) { /* process the START chunk */
		uint16_t buf[4];
		modbus_file_send_header_t *hdr;

		if (count != 4) {
			printf("ERROR: %s:: HEAD->count (%u) != 4 !!!\n", __func__, count);
			return NMBS_ERROR_INVALID_REQUEST;
		}
		for (int i = 0; i < count; i++) {
			buf[i] = __REV16(registers[i]);
		}
		hdr = (void *)buf;
		printf("%s::MOBDUS_FILE_SEND_HEAD_REC_NUM hdr->total_len: %lu, n_chunks: %lu\n",
			__func__, hdr->total_len, hdr->n_chunks);

		if (hdr->n_chunks > MOBDUS_FILE_SEND_HEAD_REC_NUM) {
			printf("ERROR: %s:: hdr->n_chunks (%lu) > %u !!!\n", __func__, hdr->n_chunks, MOBDUS_FILE_SEND_HEAD_REC_NUM);
			return NMBS_ERROR_INVALID_REQUEST;
		}
		if (hdr->total_len > MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE) {
			printf("ERROR: %s:: hdr->total_len (%lu) > %u !!!\n", __func__, hdr->total_len, MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE);
			return NMBS_ERROR_INVALID_REQUEST;
		}

		/* ESP32: check for prev modbus_file_send_data_recv_buf ptr and free it. */
		/* ESP32: malloc the memory for modbus_file_send_data_recv_buf or use a static one. */

		modbus_file_send_data_recv_buf_rest = hdr->total_len;
		modbus_file_send_data_recv_buf_ptr = modbus_file_send_data_recv_buf;

		return NMBS_ERROR_NONE;
	} else if (record_number == MOBDUS_FILE_SEND_TAIL_REC_NUM) { /* process the END chunk */
		if (modbus_file_send_data_recv_buf_ptr != NULL) {
			modbus_file_send_data_recv_buf_ptr = NULL;
			/* Processing collected data chunks. */
			printf("%s\n", (char *)modbus_file_send_data_recv_buf);

			/* ESP32: check for prev modbus_file_send_data_recv_buf ptr and free it. */
			return NMBS_ERROR_NONE;
		} else {
			printf("ERROR: %s:: recv_buf_ptr is NULL !!!\n", __func__);
			return NMBS_ERROR_INVALID_REQUEST;
		}
	} else { /* process the DATA (payload) chunk */
		if (modbus_file_send_data_recv_buf_ptr == NULL) {
			printf("ERROR: %s:: recv_buf_ptr is NULL !!!\n", __func__);
			return NMBS_ERROR_INVALID_REQUEST;
		}
		if (count * 2 > MOBDUS_PAYLOAD_CHUNK_SIZE) {
			printf("ERROR: %s:: count (%u) * 2 > %u !!!\n", __func__, count, MOBDUS_PAYLOAD_CHUNK_SIZE);
			return NMBS_ERROR_INVALID_REQUEST;
		}

		uint16_t *ptr = (void *)modbus_file_send_data_recv_buf_ptr;
		for (int i = 0; i < count; i++) {
			if (modbus_file_send_data_recv_buf_rest < 2) {
				printf("ERROR: %s:: recv_buf_rest (%lu) < 2 !!!\n",
					__func__, modbus_file_send_data_recv_buf_rest);
				return NMBS_ERROR_INVALID_REQUEST;
			} else {
				ptr[i] = __REV16(registers[i]);
				modbus_file_send_data_recv_buf_rest -= 2;
			}
		}
		modbus_file_send_data_recv_buf_ptr += count * 2;
		return NMBS_ERROR_NONE;
	}

	printf("ERROR: %s:: еhe end of the function has been reached !!!\n", __func__);
	return NMBS_ERROR_INVALID_REQUEST;
}

static int32_t read_serial(uint8_t *buf, uint16_t count,
			   int32_t byte_timeout_ms, void *arg)
{
	uint16_t received = 0;
	uint32_t start = HAL_GetTick();

	while (received < count) {
		// Compute current DMA write index
		uint16_t dma_pos =
		    UART2_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

		// Process all new bytes
		while (uart2_rx_last_pos != dma_pos && received < count) {
			buf[received++] = uart2_rx_dma_buf[uart2_rx_last_pos];

			uart2_rx_last_pos++;
			if (uart2_rx_last_pos >= UART2_RX_BUF_SIZE)
				uart2_rx_last_pos = 0;
		}

		// Timeout check.
		if (byte_timeout_ms >= 0) {
			if ((HAL_GetTick() - start) >=
			    (uint32_t)byte_timeout_ms)
				break;
		}
	}

	if (1) { /* Debug print. */
		if (received > 0) {
			printf("%s:: requested: %d, received: %d\n", __func__,
			       count, received);
			printf("%s:: data:", __func__);
			for (int i = 0; i < received; i++) {
				printf(" %02X", buf[i]);
			}
			printf("\n");
		}
	}

	return received;
}

static int32_t write_serial(const uint8_t *buf, uint16_t count,
			    int32_t byte_timeout_ms, void *arg)
{
	HAL_StatusTypeDef status =
	    HAL_UART_Transmit(&MB_UART, buf, count, byte_timeout_ms);

	if (status == HAL_OK) {
		if (1) {
			printf("%s:: count: %d, status: %d\n", __func__, count,
			       status);
			printf("%s:: data:", __func__);
			for (int i = 0; i < count; i++) {
				printf(" %02X", buf[i]);
			}
			printf("\n");
		}
	}

	if (status == HAL_OK) {
		return count;
	} else {
		return 0;
	}
}
