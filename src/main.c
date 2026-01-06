#include <stdio.h>
#include <string.h> // Required for strlen()
#include <math.h>
#include "main.h"
#include "ds18b20.h"

// Define the pin for the LED.
// Assuming PA4 as used in the previous example.
#define LED_PIN GPIO_PIN_1
#define LED_GPIO_PORT GPIOB
#define RS485_GPIO_PORT GPIOA
#define RS485_RE_DE GPIO_PIN_4
#define ONEWIRE_PORT GPIOA
#define ONEWIRE_PIN GPIO_PIN_5

#define INA226_REG_CONFIG    0x00
#define INA226_REG_SHUNTV    0x01
#define INA226_REG_BUSV      0x02
#define INA226_REG_POWER     0x03
#define INA226_REG_CURRENT   0x04
#define INA226_REG_CALIB     0x05
#define INA226_BUSV_LSB      0.00125f  // 1.25mV per LSB

ds18b20_t ds18;

typedef struct {
    float voltage; // Volts
    float current; // Amps
} INA226_Data;

static HAL_StatusTypeDef INA226_ReadData(uint16_t addr, INA226_Data *data)
{
	uint8_t reg;
	int16_t raw;
	uint8_t buffer[2];

	// 1. Read Bus Voltage (Register 0x02)
	reg = INA226_REG_BUSV;
	if (HAL_I2C_Master_Transmit(&hi2c1, (addr << 1), &reg, 1, 100) != HAL_OK) return HAL_ERROR;
	if (HAL_I2C_Master_Receive(&hi2c1, (addr << 1), buffer, 2, 100) != HAL_OK) return HAL_ERROR;

	raw = (buffer[0] << 8) | buffer[1];
	data->voltage = (float)raw * INA226_BUSV_LSB;

	// 2. Read Current (Register 0x04)
	reg = INA226_REG_CURRENT;
	if (HAL_I2C_Master_Transmit(&hi2c1, (addr << 1), &reg, 1, 100) != HAL_OK) return HAL_ERROR;
	if (HAL_I2C_Master_Receive(&hi2c1, (addr << 1), buffer, 2, 100) != HAL_OK) return HAL_ERROR;

	raw = (buffer[0] << 8) | buffer[1];
	// This conversion depends on your Calibration LSB (Assuming 1mA/LSB here)
	data->current = (float)raw * 0.001f;

	return HAL_OK;
}

static void Monitor_All_Sensors(void)
{
	uint16_t sensor_addrs[] = {0x40, 0x41, 0x42, 0x43, 0x48};
	INA226_Data sensor_readings;

	for (int i = 0; i < 5; i++) {
		if (INA226_ReadData(sensor_addrs[i], &sensor_readings) == HAL_OK) {
				// Process data, e.g., send via RS485
				// sensor_readings.voltage and sensor_readings.current are now valid
				printf("INA226-0x%x, voltage: %d.%02d, current: %d.%02d\n",
					sensor_addrs[i],
					(int)sensor_readings.voltage,
					(int)((sensor_readings.voltage - floor(sensor_readings.voltage)) * 100.),
					(int)sensor_readings.current,
					(int)((sensor_readings.current - floor(sensor_readings.current)) * 100.)
				);
		}
	}
}

static void ds18b20_init_stage1(void)
{
	ow_init_t ow_init_struct;
	ow_init_struct.tim_handle = &htim1;
	ow_init_struct.gpio = ONEWIRE_PORT;
	ow_init_struct.pin = ONEWIRE_PIN;
	ow_init_struct.tim_cb = ds18_tim_cb;
	ow_init_struct.done_cb = NULL;   // Optional
	ow_init_struct.rom_id_filter = DS18B20_ID;

	printf("DS18B20 stage1 init is start\n");
	ds18b20_init(&ds18, &ow_init_struct);
	printf("DS18B20 stage1 init is done\n");
}

static void ds18b20_init_stage2(void)
{
	printf("DS18B20 stage2 init is start\n");

	// Update ROM IDs for all devices
	ds18b20_update_rom_id(&ds18);
	printf("ds18b20_init_stage1: st%d\n", 1);
	while(ds18b20_is_busy(&ds18));
	printf("ds18b20_init_stage1: st%d\n", 2);
	printf("ds18b20->ow_devices(): %d, OW_MAX_DEVICE: %d\n",
		ow_devices(&ds18.ow), OW_MAX_DEVICE);

	// Configure alarm thresholds and resolution
	ds18b20_config_t ds18_conf = {
			.alarm_high = 90,
			.alarm_low = -50,
			.cnv_bit = DS18B20_CNV_BIT_12
	};

	ds18b20_conf(&ds18, &ds18_conf);
	while(ds18b20_is_busy(&ds18));

	printf("DS18B20 stage2 init is done\n");
}

static void ds18b20_read_temp(void)
{
	uint8_t a;
	int16_t temp_c[] = {0, 0, 0};

	// Update ROM IDs for all devices
	ds18b20_update_rom_id(&ds18);
	while(ds18b20_is_busy(&ds18));
	printf("ds18b20->ow_devices(): %d\n", ow_devices(&ds18.ow));

	ds18b20_cnv(&ds18);
	while(ds18b20_is_busy(&ds18));
	while(!ds18b20_is_cnv_done(&ds18));

	for (a = 0; a < sizeof(temp_c) / sizeof(temp_c[0]); a++) {
		uint8_t *serial = ds18.ow.rom_id[a].rom_id_struct.serial;
		ds18b20_req_read(&ds18, a);
		while(ds18b20_is_busy(&ds18));
		temp_c[a] = ds18b20_read_c(&ds18);
		printf("ds18b20_read_temp-N%d-%02x:%02x:%02x:%02x:%02x:%02x -> %d\n",
			a,
			serial[0], serial[1], serial[2],
			serial[3], serial[4], serial[5],
			temp_c[a]
		);
	}

}

void do_test_timer(void)
{
	HAL_StatusTypeDef ret;

	// printf("do_test_timer, stage: %d\n", 1);
	// delay_ms(100);
	// printf("do_test_timer, stage: %d\n", 2);
	// delay_ms(200);
	// printf("do_test_timer, stage: %d\n", 3);

	ret = HAL_TIM_RegisterCallback(&htim1, HAL_TIM_PERIOD_ELAPSED_CB_ID, ds18_tim_test_cb);
	if (ret != HAL_OK) {
		printf("HAL_TIM_RegisterCallback return error: %d !!!\n", ret);
	}

}

int main(void)
{
	/* MCU Configuration part */
	HAL_Init(); // initialize the HAL Library
	SystemClock_Config(); // configure the system clock
	MX_GPIO_Init(); // initialize GPIO pins
	MX_USART2_UART_Init(); // initialize USART2 pins
	MX_I2C1_Init(); // initialize I2C on PA9 / PA10
	MX_TIM1_Init();


	printf("STM32F070 init is done\n");

	 /* Perform the I2C Scan for target devices (0x40-0x43, 0x48) */
  //I2C_Scan_Specific();
	//do_test_timer();

	int loop_count = 0;

	/* Infinite loop. */
	while (1) {
		// Turn the LED on.
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
		HAL_Delay(500); // wait 500 milliseconds

		// Turn the LED off.
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
		HAL_Delay(500); // wait 500 milliseconds

		printf("Hello from STM32! Loop count is: %d\n", loop_count++);
		Monitor_All_Sensors();
		ds18b20_read_temp();
	}
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  // Timing value for 100kHz @ 48MHz internal clock
  hi2c1.Init.Timing = 0x20303E5D;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  HAL_I2C_Init(&hi2c1);
}

 /**
		* I2C1 GPIO Configuration
    * 	PA9     ------> I2C1_SCL
  	* 	PA10    ------> I2C1_SDA
    */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hi2c->Instance == I2C1) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      // I2C requires Open Drain
    GPIO_InitStruct.Pull = GPIO_PULLUP;        // Internal pull-ups (External still recommended)
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;   // AF4 for PA9/PA10
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

static void I2C_Scan_Specific(void)
{
  for (uint8_t target_address = 1; target_address < 128; target_address++) {
    // HAL_I2C_IsDeviceReady expects (address << 1)
    if (HAL_I2C_IsDeviceReady(&hi2c1, (target_address << 1), 3, 10) == HAL_OK) {
      // Device found! You can add your logic here (e.g., UART print)
			printf("Device with address: 0x%x is found!\n", target_address);
    }
  }
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

	/* Configure 1Wire Pin as output */
	GPIO_InitStruct.Pin = ONEWIRE_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Open Drain is mandatory for 1-Wire
	GPIO_InitStruct.Pull = GPIO_PULLUP;        // Internal pull-up is okay alongside your 4k7
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(ONEWIRE_PORT, &GPIO_InitStruct);

}

/* Put this in a .c file (e.g., tim1_us.c) and declare htim1 in a header or main.c */
#include "stm32f0xx_hal.h"

TIM_HandleTypeDef htim1;

/* Call this from main after HAL_Init() and SystemClock_Config() */
void MX_TIM1_Init(void)
{
  /* Enable TIM1 clock */
  __HAL_RCC_TIM1_CLK_ENABLE();

  /* Compute prescaler to get 1 MHz timer clock (1 us tick)
     Prescaler = (TimerClock / 1,000,000) - 1
     TimerClock is SystemCoreClock for TIM1 on F0 (APB prescaler = 1 typically).
  */
  uint32_t prescaler = (SystemCoreClock / 1000000U) - 1U;
	printf("MX_TIM1_Init::SystemCoreClock: %u, prescaler: %u\n",
		SystemCoreClock, prescaler);

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = prescaler;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xFFFF;               /* 16-bit auto-reload */
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;

  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		printf("Error calling HAL_TIM_Base_Init !!!\n");
	}

	/* Configure the NVIC for TIM1 Update Interrupt. */
	HAL_NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

	//do_test_timer();
	ds18b20_init_stage1();

  /* Start the timer base */
  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK) {
		printf("Error calling HAL_TIM_Base_Start_IT !!!\n");
	}

	ds18b20_init_stage2();
}

/* Microsecond delay using TIM1 (handles 16-bit wrap) */
void delay_us(uint32_t us)
{
  uint32_t start = __HAL_TIM_GET_COUNTER(&htim1);
  /* Wait until the required time has elapsed, handle wrap-around */
  while (((__HAL_TIM_GET_COUNTER(&htim1) - start) & 0xFFFF) < (us & 0xFFFF)) {
    /* busy wait */
  }
}

/* Millisecond delay wrapper */
void delay_ms(uint32_t ms)
{
  while (ms--) {
    delay_us(1000);
  }
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
		HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len - 1, 100);
		HAL_UART_Transmit(&huart2, (uint8_t*)&cr, strlen(cr), 100);
	} else {
		HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
	}
	return len;
}

/**
  * @brief This function handles TIM1 Break, Update, Trigger and Commutation interrupts.
  */
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

void ds18_tim_cb(TIM_HandleTypeDef *htim)
{
	//printf("ds18_tim_cb !!!\n");
	ow_callback(&ds18.ow);
}

void ds18_tim_test_cb(TIM_HandleTypeDef *htim)
{
	printf("ds18_tim_test_cb !!!\n");
}
