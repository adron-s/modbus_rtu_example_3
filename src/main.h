#ifndef __MAIN_H
#define __MAIN_H
#include "stm32f0xx_hal.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPIO_Repurpose_SWDIO(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void I2C_Scan_Specific(void);

UART_HandleTypeDef huart2; // Handle for UART2 configuration
I2C_HandleTypeDef hi2c1;

#endif /* __MAIN_H */
