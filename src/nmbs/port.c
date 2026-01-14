#include "nmbs/port.h"
#include <stdio.h>

/* Variables required for the modbus file send algorithm to work. */
typedef struct {
	uint8_t *recv_buf;
	uint8_t *recv_buf_ptr;
	uint32_t recv_buf_rest;
} modbus_file_send_data_ctx_t;

/* Context variables for the modbus file send algorithm to work. */
modbus_file_send_data_ctx_t modbus_file_send_data_ctx = {NULL, NULL, 0};
/* Static buffer for receiving data in the modbus file send algorithm. */
uint8_t _modbus_file_send_data_recv_buf[MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE];

/*
	Modbus holding registers read handler.
*/
static nmbs_error server_read_holding_registers(uint16_t address,
						uint16_t quantity,
						uint16_t *registers_out,
						uint8_t unit_id, void *arg)
{
	printd("address: %d, unit_id: %d\n", address, unit_id);

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

/*
	Modbus multiple registers write handler.
*/
static nmbs_error server_write_multiple_registers(uint16_t address,
						  uint16_t quantity,
						  const uint16_t *registers,
						  uint8_t unit_id, void *arg)
{
	printd("address: %d, quantity: %d, unit_id: %d\n", address, quantity,
	       unit_id);

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

/*
	Modbus single register write handler.
*/
static nmbs_error server_write_single_register(uint16_t address, uint16_t value,
					       uint8_t unit_id, void *arg)
{
	uint16_t reg = value;
	return server_write_multiple_registers(address, 1, &reg, unit_id, arg);
}

/*
	Performs ~HEAD chunk~ processing for the modbus file send algorithm.
*/
static nmbs_error
modbus_file_send_process_head_chunk(modbus_file_send_data_ctx_t *ctx,
				    const uint16_t *registers, uint16_t count)
{

	modbus_file_send_header_t *hdr;
	uint16_t buf[sizeof(*hdr) / 2];

	if (count * 2 != sizeof(*hdr)) {
		printe("count (%u) != %u", count, sizeof(*hdr) / 2);
		return NMBS_ERROR_INVALID_REQUEST;
	}

	/* Copy and change the byte order (since in modbus registers it is
	 * always big-endian). */
	for (size_t i = 0; i < count; i++) {
		buf[i] = SWAP16_IF_LE(registers[i]);
	}
	hdr = (void *)buf;
	printd("total_len: %lu, n_chunks: %lu\n", hdr->total_len,
	       hdr->n_chunks);

	if (hdr->n_chunks > MOBDUS_FILE_SEND_HEAD_REC_NUM) {
		printe("hdr->n_chunks (%lu) > %u", hdr->n_chunks,
		       MOBDUS_FILE_SEND_HEAD_REC_NUM);
		return NMBS_ERROR_INVALID_REQUEST;
	}
	if (hdr->total_len > MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE) {
		printe("hdr->total_len (%lu) > %u", hdr->total_len,
		       MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE);
		return NMBS_ERROR_INVALID_REQUEST;
	}

	/* If the previous data transfer session was not completed correctly, we
	 * perform a cleanup. */
	if (ctx->recv_buf != NULL || ctx->recv_buf_ptr != NULL ||
	    ctx->recv_buf_rest != 0) {

		if (ctx->recv_buf_ptr != NULL) {
			printe("recv_buf_ptr is not NULL");
		}
		if (ctx->recv_buf_rest != 0) {
			printe("recv_buf_rest (%lu) != 0", ctx->recv_buf_rest);
		}

		ctx->recv_buf_rest = 0;
		ctx->recv_buf_ptr = NULL;

		if (ctx->recv_buf != NULL) {
			/* TODO: free (ctx->recv_buf) !!! */
			ctx->recv_buf = NULL;
			printe("recv_buf is not NULL");
		}
	}

	/* TODO: alloc memory for data_recv_buf (or use a static one) !!! */
	ctx->recv_buf = _modbus_file_send_data_recv_buf;
	ctx->recv_buf_ptr = ctx->recv_buf;
	ctx->recv_buf_rest = hdr->total_len;

	return NMBS_ERROR_NONE;
}

/*
	Performs ~TAIL chunk~ processing for the modbus file send algorithm.
*/
static nmbs_error
modbus_file_send_process_tail_chunk(modbus_file_send_data_ctx_t *ctx)
{
	char *recv_buf = (void *)ctx->recv_buf;

	if (ctx->recv_buf_ptr != NULL) {
		ctx->recv_buf_ptr = NULL;
		if (ctx->recv_buf_rest != 0) {
			printe("recv_buf_rest (%lu) != 0", ctx->recv_buf_rest);
			ctx->recv_buf_rest = 0;
			return NMBS_ERROR_INVALID_REQUEST;
		}

		if (recv_buf != NULL) {
			/* All data has been received - we are passing it on for
			 * the next processing. */
			ctx->recv_buf = NULL;
			printf("%s\n", recv_buf);
			/* TODO: free(recv_buf) if necessary !!! */
			return NMBS_ERROR_NONE; /* all is done ok */
		} else {
			printe("recv_buf is NULL");
			return NMBS_ERROR_INVALID_REQUEST;
		}
	} else {
		ctx->recv_buf_rest = 0;
		printe("recv_buf_ptr is NULL");
		return NMBS_ERROR_INVALID_REQUEST;
	}
}

/*
	Performs ~DATA chunk~ processing for the modbus file send algorithm.
*/
static nmbs_error
modbus_file_send_process_data_chunk(modbus_file_send_data_ctx_t *ctx,
				    const uint16_t *registers, uint16_t count)
{
	uint16_t *ptr = (void *)ctx->recv_buf_ptr;

	if (ptr == NULL) {
		printe("recv_buf_ptr is NULL");
		return NMBS_ERROR_INVALID_REQUEST;
	}
	if (count * 2 > MOBDUS_PAYLOAD_CHUNK_SIZE) {
		printe("count (%u) * 2 > %u", count, MOBDUS_PAYLOAD_CHUNK_SIZE);
		return NMBS_ERROR_INVALID_REQUEST;
	}

	for (int i = 0; i < count; i++) {
		if (ctx->recv_buf_rest < 2) {
			printe("recv_buf_rest (%lu) < 2", ctx->recv_buf_rest);
			return NMBS_ERROR_INVALID_REQUEST;
		} else {
			ptr[i] = SWAP16_IF_LE(registers[i]);
			ctx->recv_buf_rest -= 2;
		}
	}
	ctx->recv_buf_ptr += count * 2;
	return NMBS_ERROR_NONE;
}

/*
	Implementation of receiving a large file (json) using modbus chunks
	and then assembling them into a single text (json) message.
*/
static nmbs_error server_write_file_record(uint16_t file_number,
					   uint16_t record_number,
					   const uint16_t *registers,
					   uint16_t count, uint8_t unit_id,
					   void *arg)
{
	modbus_file_send_data_ctx_t *ctx = &modbus_file_send_data_ctx;

	printd("file_number: %d, record_number: %d, count: %d, "
	       "unit_id: %d\n",
	       file_number, record_number, count, unit_id);

	if (record_number > MOBDUS_FILE_SEND_TAIL_REC_NUM) {
		printe("invalid record_number (%u)", record_number);
		return NMBS_ERROR_INVALID_REQUEST;
	}

	switch (record_number) {
		case MOBDUS_FILE_SEND_HEAD_REC_NUM:
			return modbus_file_send_process_head_chunk(
			    ctx, registers, count);
		case MOBDUS_FILE_SEND_TAIL_REC_NUM:
			return modbus_file_send_process_tail_chunk(ctx);
		default:
			return modbus_file_send_process_data_chunk(
			    ctx, registers, count);
	}
}

/*
	Platform-specific handler for the modbus library callable
	to read data of a given size from serial.
*/
static int32_t read_serial(uint8_t *buf, uint16_t count,
			   int32_t byte_timeout_ms, void *arg)
{
	uint16_t dma_pos;
	uint16_t received = 0;
	uint32_t start = HAL_GetTick();

	while (received < count) {
		/* Compute current DMA write index. */
		dma_pos = UART2_RX_DMA_BUF_SIZE -
			  __HAL_DMA_GET_COUNTER(huart2.hdmarx);

		/* Process all new bytes. */
		while (uart2_rx_dma_last_pos != dma_pos && received < count) {
			buf[received++] =
			    uart2_rx_dma_buf[uart2_rx_dma_last_pos];

			uart2_rx_dma_last_pos++;
			if (uart2_rx_dma_last_pos >= UART2_RX_DMA_BUF_SIZE)
				uart2_rx_dma_last_pos = 0;
		}

		/* Timeout check. */
		if (byte_timeout_ms >= 0) {
			if ((HAL_GetTick() - start) >=
			    (uint32_t)byte_timeout_ms)
				break;
		}

		/* If you use FreeRTOS, add HAL_Delay() here to allow other
		 * threads to work while this code sleeps. */
	}

	if (DEBUG_SERIAL_DATA_FLOWS) {
		if (received > 0) {
			printd("count: %d, received: %d\n", count, received);
			data_print(buf, received);
		}
	}

	return received;
}

/*
	Platform-specific handler for the modbus library callable
	for writing data of a specified size to a serial port.
*/
static int32_t write_serial(const uint8_t *buf, uint16_t count,
			    int32_t byte_timeout_ms, void *arg)
{
	HAL_StatusTypeDef status =
	    HAL_UART_Transmit(&MB_UART, buf, count, byte_timeout_ms);

	if (status == HAL_OK) {
		if (DEBUG_SERIAL_DATA_FLOWS) {
			printd("count: %d, status: %d\n", count, status);
			data_print(buf, count);
		}
	}

	if (status == HAL_OK) {
		return count;
	} else {
		return 0;
	}
}

/*
	Initializes a nanomodbus instance.
*/
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
