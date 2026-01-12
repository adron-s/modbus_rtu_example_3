#include <stdio.h>
#include <string.h> // Required for strlen()
#include <math.h>
#include "nanomodbus.h"
#include "nmbs/port.h"
#include "main.h"
#include "tim.h"

#define LED_PIN GPIO_PIN_4
#define LED_GPIO_PORT GPIOA
#define RS485_GPIO_PORT GPIOA
#define RS485_RE_DE GPIO_PIN_1

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

UART_HandleTypeDef huart1; // Handle for UART1 configuration
UART_HandleTypeDef huart2; // Handle for UART1 configuration

DMA_HandleTypeDef hdma_usart2_rx;

uint8_t uart2_rx_dma_buf[UART2_RX_BUF_SIZE]; // DMA circular buffer
volatile uint16_t uart2_rx_last_pos = 0; // last processed position

nmbs_t nmbs;
const uint8_t nmbs_id = 0x01;

void modbus_server_init(void)
{
	nmbs_server_init(&nmbs, nmbs_id);
}

void uart_dma_process(void) {
	uint16_t dma_pos = UART2_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

	if (uart2_rx_last_pos != dma_pos) {
		printf("%s::data>", __func__);

		do {
			uint8_t b = uart2_rx_dma_buf[uart2_rx_last_pos];

			// Print received byte
			printf(" %02x", b);

			uart2_rx_last_pos++;
			if (uart2_rx_last_pos >= UART2_RX_BUF_SIZE) {
				uart2_rx_last_pos = 0;
			}
		} while (uart2_rx_last_pos != dma_pos);

		printf("\n");
	}
}

void do_uart2_poll(void)
{
	// uint8_t buf[32];
	// uint16_t count = 1;

	if (1) {
		// Manual way to clear ORE on STM32F0 if macros aren't working:
		// if (USART2->ISR & USART_ISR_ORE) {
		// 	printf("USART2->ISR & USART_ISR_ORE !!!\n");
		//  	USART2->ICR = USART_ICR_ORECF; // Clear the ORE flag
		// }

		printf("_HAL_DMA_GET_COUNTER(huart2.hdmarx): %lu\n", __HAL_DMA_GET_COUNTER(huart2.hdmarx));
		uart_dma_process();
		// HAL_StatusTypeDef status = HAL_UART_Receive(&huart2, buf, count, 100);
		// if (status == HAL_OK) {
		// 	printf("%s::data> ", __func__);
		// 	for (int a = 0; a < count; a++) {
		// 		printf(" %02X", buf[a]);
		// 	}
		// 	printf("\n");
		// }
	}
}

int loop_count = 0;
int main(void)
{
	/* MCU Configuration part */
	HAL_Init(); // initialize the HAL Library
	SystemClock_Config(); // configure the system clock
	MX_USART1_UART_Init(); // initialize USART1
	printf("USART1-debug init is done\n");
	MX_DMA_Init();
	MX_USART2_UART_Init(); // initialize USART2
	MX_GPIO_Init(); // initialize GPIO pins
	modbus_server_init();

	printf("STM32F070 init is done\n");

	if (0) {
		char msg[] = "Hello USART2\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
	}

	/* Infinite loop. */
	while (1) {
		// Turn the LED off.
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
		HAL_Delay(100); // wait 500 milliseconds
		// Turn the LED on.
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
		HAL_Delay(100); // wait 500 milliseconds

		nmbs_server_poll(&nmbs);

		printf("Hello from STM32! Loop count is: %d\n", loop_count);
		loop_count++;
		//do_uart2_poll();
	}
}

/**
	* USART1 GPIO Configuration.
	*		PA9     ------> USART1_TX
	*		PA10    ------> USART1_RX
	*/
static void MX_USART1_UART_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_USART1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF1_USART1; // This is the "secret sauce" for AF
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

	HAL_UART_Init(&huart1);
}

/**
	* USART2 GPIO Configuration.
	*		PA2     ------> USART2_TX
	*		PA3    ------> USART2_RX
	*/
static void MX_USART2_UART_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF1_USART2; // This is the "secret sauce" for AF
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

	HAL_UART_Init(&huart2);

	HAL_UART_Receive_DMA(&huart2, uart2_rx_dma_buf, UART2_RX_BUF_SIZE);
	/* Force DMA into circular mode. */
	__HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);   // optional: disable half-transfer IRQ
	huart2.hdmarx->Instance->CCR |= DMA_CCR_CIRC;     // enable circular mode
}

void MX_DMA_Init(void)
{
	__HAL_RCC_DMA1_CLK_ENABLE();

	hdma_usart2_rx.Instance = DMA1_Channel5;
	hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
	hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hdma_usart2_rx.Init.Mode = DMA_CIRCULAR;
	hdma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
	HAL_DMA_Init(&hdma_usart2_rx);

	__HAL_LINKDMA(&huart2, hdmarx, hdma_usart2_rx);
}


// --- CubeMX Generated Configuration Functions ---
// These functions set up the clock and GPIO peripherals.
// They are required to make the chip work correctly.

/**
	* @brief System Clock Configuration
	* Configures the system clock source, flash latency, etc.
	*/
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/* Phase 1: Setting the Source (The Oscillator). */
	/* The following config is standard for an F030 using internal HSI. */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI; // use Internal High Speed
	RCC_OscInitStruct.HSIState = RCC_HSI_ON; // turn it ON
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	//RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; // using HSI directly - don't use the Multiplier (PLL)
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON; // change PLLState to ON - for 48Mhz clock !

	// Set HSI as source for PLL (usually divided by 2 first on some F0s,
  // but on F070 we set it to result in 48MHz)
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6; // 8MHz * 6 = 48MHz

	HAL_RCC_OscConfig(&RCC_OscInitStruct); // apply settings

	/* Phase 2: Routing the Clock (The Bus). */
	/*
		SYSCLK: The main clock for the CPU core.
		HCLK (AHB Bus): Controls high-speed peripherals and memory access.
		PCLK1 (APB Bus): Controls slower peripherals like GPIOs, Timers, and UART.
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
															|RCC_CLOCKTYPE_PCLK1;
	//RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI; // set System clock to HSI (8Mhz) - as the main System Clock
	// Change Source from HSI to PLLCLK
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // set System clock to PLL - for 48Mhz clock !
	/* Phase 3: Dividers and Latency. */
	/* Dividers: You can slow down specific parts of the chip to save power.
		 Setting them to DIV1 means they run at the full 8MHz. */
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // don't slow down AHB
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1; // don't slow down APB
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0); // use Latency 0 for maximum flash memory speed.
}

/**
	* @brief GPIO Initialization Function
	* Enables the GPIOA clock and configures the LED pin as output push-pull.
	*
	* This function replaces the Arduino pinMode() function.
	*/
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	/* In an STM32, every peripheral (GPIO, Timers, UART) is turned off by default.
		 If you try to write to a pin without enabling its "Clock" (the power/timing signal for that port),
		 the command will be ignored. This line wakes up Port A. */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* Configure GPIO pin Output Level */
	/* This ensures the LED is OFF (Reset) the very moment the pin is activated.
		 Without this, the pin might "glitch" or stay in an unknown state until
		 your while(1) loop starts. */
	HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);

	/* SP3485EN-L/TR direction - LOW is for RX, HIGH is for TX. */
	HAL_GPIO_WritePin(RS485_GPIO_PORT, RS485_RE_DE, GPIO_PIN_SET);

	/* Configure LED Pin as output */
	GPIO_InitStruct.Pin = LED_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

	/* Configure RS485_RE_DE Pin as output */
	GPIO_InitStruct.Pin = RS485_RE_DE;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RS485_GPIO_PORT, &GPIO_InitStruct);
}

/**
	* Required callback function by HAL, leave empty if not used.
	* This is runs (every 1ms).
	*/
void SysTick_Handler(void) {
	/* This code is required for HAL_Delay() to work correctly. */
	HAL_IncTick(); // simply adds +1 to that global counter (uwTick)
}

/**
	* This is the "magic" function that connects printf to UART.
	*/
int _write(int file, char *ptr, int len) {
	static const char cr[] = "\r\n";

	/* Timeout set to 100ms. */
	if (ptr[len - 1] == '\n') {
		HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len - 1, 100);
		HAL_UART_Transmit(&huart1, (uint8_t*)&cr, strlen(cr), 100);
	} else {
		HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, 100);
	}
	return len;
}
