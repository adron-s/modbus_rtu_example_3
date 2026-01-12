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

nmbs_error nmbs_server_init(nmbs_t *, const uint8_t);

#endif /* NANOMODBUS_CONFIG_H */
