#ifndef NANOMODBUS_CONFIG_H
#define NANOMODBUS_CONFIG_H

#include "main.h"

#define UART2_RX_BUF_SIZE 256
extern uint8_t uart2_rx_dma_buf[UART2_RX_BUF_SIZE]; // DMA circular buffer
extern volatile uint16_t uart2_rx_last_pos; // last processed position
extern int loop_count;

// NanoModbus include
#include "nanomodbus.h"

// modbus rtu
#define MB_UART huart2
extern UART_HandleTypeDef MB_UART;

#define MOBDUS_PAYLOAD_CHUNK_SIZE 240
#define MOBDUS_FILE_SEND_HEAD_REC_NUM 9998
#define MOBDUS_FILE_SEND_TAIL_REC_NUM 9999
#define MODBUS_FILE_SEND_DATA_RECV_BUF_SIZE 2048 //2kb of RAM

typedef struct {
	uint32_t total_len;
	uint32_t n_chunks;
} modbus_file_send_header_t;

nmbs_error nmbs_server_init(nmbs_t *, const uint8_t);

#endif /* NANOMODBUS_CONFIG_H */
