#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "main.h"
#include "nmbs/port.h"

static int32_t read_serial(
	uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg
);
static int32_t write_serial(
	const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg
);

static nmbs_error server_read_holding_registers(
	uint16_t address, uint16_t quantity, uint16_t* registers_out,
	uint8_t unit_id, void* arg
);

nmbs_error nmbs_server_init(nmbs_t* nmbs, const uint8_t nmbs_id)
{
	nmbs_platform_conf conf;
	nmbs_callbacks cb;

	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_RTU;
	conf.read = read_serial;
	conf.write = write_serial;


	nmbs_callbacks_create(&cb);
	cb.read_holding_registers = server_read_holding_registers;
	// cb.write_single_register = server_write_single_register;
	// cb.write_multiple_registers = server_write_multiple_registers;

	nmbs_error status = nmbs_server_create(nmbs, nmbs_id, &conf, &cb);
	if (status != NMBS_ERROR_NONE) {
		return status;
	}

	nmbs_set_byte_timeout(nmbs, 100);
	nmbs_set_read_timeout(nmbs, 200);

	return NMBS_ERROR_NONE;
}

static nmbs_error server_read_holding_registers(
	uint16_t address, uint16_t quantity, uint16_t* registers_out,
	uint8_t unit_id, void* arg
) {
	printf("%s, address: %d, unit_id: %d\n", __func__, address, unit_id);

	for (size_t i = 0; i < quantity; i++) {
		uint16_t cur_reg = address + i;

		switch (cur_reg) {
			case 0:
				registers_out[i] = loop_count;
				break;

			default:
				return NMBS_ERROR_INVALID_REQUEST;
		}
	}
	return NMBS_ERROR_NONE;
}

static int32_t read_serial(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg)
{
	uint16_t received = 0;
	uint32_t start = HAL_GetTick();

	while (received < count) {
		// Compute current DMA write index
		uint16_t dma_pos = UART2_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

		// Process all new bytes
		while (uart2_rx_last_pos != dma_pos && received < count) {
			buf[received++] = uart2_rx_dma_buf[uart2_rx_last_pos];

			uart2_rx_last_pos++;
			if (uart2_rx_last_pos >= UART2_RX_BUF_SIZE)
				uart2_rx_last_pos = 0;
		}

		// Timeout check.
		if (byte_timeout_ms >= 0)
		{
			if ((HAL_GetTick() - start) >= (uint32_t)byte_timeout_ms)
				break;
		}
	}

	if (1) { /* Debug print. */
		if (received > 0) {
			printf("%s:: requested: %d, received: %d\n", __func__, count, received);
			printf("%s:: data:", __func__);
			for (int i = 0; i < received; i++) {
				printf(" %02X", buf[i]);
			}
			printf("\n");
		}
	}

	return received;
}

static int32_t write_serial(
	const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms, void *arg
) {
	HAL_StatusTypeDef status = HAL_UART_Transmit(&MB_UART, buf, count, byte_timeout_ms);

	if (status == HAL_OK) {
		if (1) {
			printf("%s:: count: %d, status: %d\n", __func__, count, status);
			printf("%s:: data:", __func__);
			for (int i = 0; i < count; i++) {
				printf(" %02X", buf[i]);
			}
			printf("\n");
		}
	}

	if (status == HAL_OK) {
		return count;
	}	else {
		return 0;
	}
}
