#include "stm32f0xx_hal.h"

// Define the pin for the LED.
// Assuming PA4 as used in the previous example.
#define LED_PIN GPIO_PIN_4
#define LED_GPIO_PORT GPIOA

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

int main(void)
{
  /* MCU Configuration part */
  HAL_Init(); // Initialize the HAL Library
  SystemClock_Config(); // Configure the system clock
  MX_GPIO_Init(); // Initialize GPIO pins

  /* Infinite loop */
  while (1)
  {
    // Turn the LED on
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
    HAL_Delay(200); // Wait 500 milliseconds

    // Turn the LED off
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    HAL_Delay(200); // Wait 500 milliseconds
  }
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; // using HSI directly - don't use the Multiplier (PLL)
  HAL_RCC_OscConfig(&RCC_OscInitStruct); // apply settings

	/* Phase 2: Routing the Clock (The Bus). */
	/*
			SYSCLK: The main clock for the CPU core.
			HCLK (AHB Bus): Controls high-speed peripherals and memory access.
			PCLK1 (APB Bus): Controls slower peripherals like GPIOs, Timers, and UART.
	*/
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI; // set System clock to HSI (8Mhz) - as the main System Clock
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

  /* Configure GPIO pin Output Level */
	/* This ensures the LED is OFF (Reset) the very moment the pin is activated.
		 Without this, the pin might "glitch" or stay in an unknown state until
		 your while(1) loop starts. */
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);

  /* Configure LED Pin as output */
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

/* Required callback function by HAL, leave empty if not used. */
/* This is runs (every 1ms) */
void SysTick_Handler(void) {
	/* This code is required for HAL_Delay() to work correctly. */
  HAL_IncTick(); // simply adds +1 to that global counter (uwTick)
}
