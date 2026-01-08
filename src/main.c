#include <stdio.h>
#include <string.h> // Required for strlen()
#include <math.h>
#include "main.h"
#include "tim.h"
#include "ds18b20.h"

// Define the pin for the LED.
// Assuming PA4 as used in the previous example.
#define LED_PIN GPIO_PIN_1
#define LED_GPIO_PORT GPIOB
#define RS485_GPIO_PORT GPIOA
#define RS485_RE_DE GPIO_PIN_4
#define ONEWIRE_PORT GPIOA
#define ONEWIRE_PIN GPIO_PIN_5
#define FANS_PWR_CTRL_PIN GPIO_PIN_13
#define FAN2_PWM_PIN GPIO_PIN_6

#define FAN_PWN_MAX_SPEED_VAL 0 //0
#define FAN_PWN_MIN_SPEED_VAL 1920 //1520
#define FAN_PWN_OFF_SPEED_VAL 1920
#define FAN2_PWM_START_VAL FAN_PWN_MIN_SPEED_VAL

#define INA226_REG_CONFIG    0x00
#define INA226_REG_SHUNTV    0x01
#define INA226_REG_BUSV      0x02
#define INA226_REG_POWER     0x03
#define INA226_REG_CURRENT   0x04
#define INA226_REG_CALIB     0x05
#define INA226_BUSV_LSB      0.00125f  // 1.25mV per LSB

#define INA226_CURRENT_25MOHM_LSB  0.0002f // 25mΩ / 2 shunt = 0.0002
#define INA226_CURRENT_100MOHM_LSB 0.000025f // 100mΩ shunt

volatile uint32_t capture_counter = 0;
volatile uint32_t last_capture = 0;
volatile uint32_t tach_period_us = 0;

ds18b20_t ds18;

typedef struct {
    float voltage; // Volts
    float current; // Amps
} INA226_Data;

/**
 * @brief  Writes a 16-bit value to an INA226 register
 * @param  hi2c: Pointer to I2C handle
 * @param  DevAddress: I2C device address (shifted left by 1)
 * @param  value: The 16-bit calibration value to program
 */
HAL_StatusTypeDef INA226_Init(uint16_t addr) {
	HAL_StatusTypeDef ret;
	uint8_t data[3];
	uint16_t value = 2048;

	if (addr != 0x48) { /* 25mΩ shunt */
		value = 3088;
	}

	// 1st byte: Register address
	data[0] = INA226_REG_CALIB;

	// 2nd byte: MSB of the value
	data[1] = (value >> 8) & 0xFF;

	// 3rd byte: LSB of the value
	data[2] = value & 0xFF;

	// Send 3 bytes (1 address + 2 data)
	ret = HAL_I2C_Master_Transmit(&hi2c1, (addr << 1), data, 3, 100);
	if (ret != HAL_OK) {
		return ret;
	}

	/* 0x4927: Default settings + 128 samples averaging (so that the readings don't jump!) */
	uint16_t config_value = 0x4927;

	data[0] = INA226_REG_CONFIG;
	data[1] = (config_value >> 8) & 0xFF; // MSB
	data[2] = config_value & 0xFF;        // LSB

	return HAL_I2C_Master_Transmit(&hi2c1, (addr << 1), data, 3, 100);
}

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
	//reg = INA226_REG_SHUNTV;
	reg = INA226_REG_CURRENT;
	if (HAL_I2C_Master_Transmit(&hi2c1, (addr << 1), &reg, 1, 100) != HAL_OK) return HAL_ERROR;
	if (HAL_I2C_Master_Receive(&hi2c1, (addr << 1), buffer, 2, 100) != HAL_OK) return HAL_ERROR;

	raw = (buffer[0] << 8) | buffer[1];
	// This conversion depends on your Calibration LSB (Assuming 1mA/LSB here)
	//printf("INA226-%d, raw current: %u\n", addr, raw);
	if (raw < 0) {
		raw = 0;
	}
	if (addr != 0x48) { /* 25mΩ shunt */
		data->current = (float)raw * INA226_CURRENT_25MOHM_LSB;
	} else { /* 100mΩ shunt */
		data->current = (float)raw * INA226_CURRENT_100MOHM_LSB;
	}

	return HAL_OK;
}

static void Monitor_All_Sensors(void)
{
	HAL_StatusTypeDef ret;
	uint16_t sensor_addrs[] = {0x40, 0x41, 0x42, 0x43, 0x48};
	static uint8_t is_init_needed[] = {1, 1, 1, 1, 1};
	INA226_Data sensor_readings;

	for (int i = 0; i < 5; i++) {
		if (is_init_needed[i]) {
			is_init_needed[i] = 0;
			ret = INA226_Init(sensor_addrs[i]);
			if (ret != HAL_OK) {
				printf("Calibrating INA226-%x, ERROR: %d !!!\n", sensor_addrs[i], ret);
			} else {
				printf("Calibrating INA226-%x - OK\n", sensor_addrs[i]);
			}
		}

		// const int av_steps = 1;
		// sensor_readings.voltage = 0;
		// sensor_readings.current = 0;
		// for (int k = 0; k < av_steps; k++) {
		// 	if (INA226_ReadData(sensor_addrs[i], &sensor_readings) != HAL_OK) {
		// 		break;
		// 	}
		// }
		// sensor_readings.voltage /= (float)av_steps;
		// sensor_readings.current /= (float)av_steps;

		// Process data, e.g., send via RS485
		// sensor_readings.voltage and sensor_readings.current are now valid
		if (INA226_ReadData(sensor_addrs[i], &sensor_readings) == HAL_OK) {
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
	int16_t temp_c[] = {0, 0, 0, 0};

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

uint32_t FAN2_GetRPM(void)
{
	uint32_t ticks = tach_period_us;  // actually ticks, not microseconds
	if (ticks == 0) {
		return 0;
	}

	return 300000UL / ticks;   // 10 kHz timer, 2 pulses per rev
}

void do_FAN2_RPM_measure(void)
{
	HAL_NVIC_DisableIRQ(TIM14_IRQn);
	/* Hardware Barrier (CRITICAL) */
	/* This ensures the 'Disable' command is fully completed in the CPU
		 hardware before the next line of code runs. */
	__DSB();
	__ISB();

	/** --- START CRITICAL SECTION --- **/
	last_capture = 0;
	tach_period_us = 0;
	/** --- END CRITICAL SECTION --- **/
	HAL_NVIC_EnableIRQ(TIM14_IRQn);

	uint32_t start_tick = HAL_GetTick();
	while (tach_period_us == 0) {
		/* If no pulse for 200ms (5Hz / 150 RPM), the fan is likely stopped. */
		if ((HAL_GetTick() - start_tick) > 200) {
			printf("%s::fan is stuck! emergency break!\n", __func__);
  		break;
    }
	}

	HAL_NVIC_DisableIRQ(TIM14_IRQn);
	__DSB();
	__ISB();
}

int main(void)
{
	int swdio_need_repurpose = 1;
	int fan2_pwm_inc_step = 100;
	int fan2_pwm_val = FAN2_PWM_START_VAL;
	uint32_t last_capture_counter = 0;

	/* MCU Configuration part */
	HAL_Init(); // initialize the HAL Library
	SystemClock_Config(); // configure the system clock
	MX_USART2_UART_Init(); // initialize USART2 pins
	MX_GPIO_Init(); // initialize GPIO pins
	MX_I2C1_Init(); // initialize I2C on PA9 / PA10
	MX_TIM1_Init();
	MX_TIM3_Init();
	MX_TIM14_Init();

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


		if (0) {
			if ((loop_count & 0x03) == 0) {
				if (swdio_need_repurpose) {
					swdio_need_repurpose = 0;
					MX_GPIO_Repurpose_SWDIO();
				}
				HAL_GPIO_TogglePin(GPIOA, FANS_PWR_CTRL_PIN);
			}
		}

		if (1) {
			if (1) {
				int val = fan2_pwm_val + fan2_pwm_inc_step;
				if (val > FAN_PWN_MIN_SPEED_VAL) {
					fan2_pwm_inc_step *= -1;
					val = FAN_PWN_MIN_SPEED_VAL;
				} else if (val < FAN_PWN_MAX_SPEED_VAL) {
					fan2_pwm_inc_step *= -1;
					val = FAN_PWN_MAX_SPEED_VAL;
				}
				// Set fan to XX% speed (0..1920)
				//val = 1740;
				__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, val);
				fan2_pwm_val = val;
				printf("Setting TIM3->CH1 PWM val := %d\n", val);
			}

			uint32_t cur_capture_count = capture_counter;
			/* We start the RPM measurement process (timer interrupt handler) only for a moment,
				 so as not to interfere with the operation of other systems. */
			do_FAN2_RPM_measure();
			printf("Current RPM := %u, capture_counter delta := %u\n",
				FAN2_GetRPM(), cur_capture_count - last_capture_counter);
			// printf("RAW value1: %u\n", HAL_TIM_ReadCapturedValue(&htim14, TIM_CHANNEL_1));
			// printf("RAW value2: %u\n", TIM14->CNT);
			last_capture_counter = cur_capture_count;
		}
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

	/* TIM3 GPIO Configuration
  		PA6     ------> TIM3_CH1
  */
	GPIO_InitStruct.Pin = FAN2_PWM_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate Function Push-Pull
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;    // THIS IS THE AF1 ASSIGNMENT
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* Configure PA7 for TIM14_CH1 Input Capture (AF4) */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate Function Push-Pull
	GPIO_InitStruct.Pull = GPIO_PULLUP;          // Fan tachometers are open-collector
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_TIM14;    // Mapping PA7 to TIM14_CH1
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_GPIO_Repurpose_SWDIO(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	  /* Configure FANS_PWR_CTRL_PIN as Output */
  GPIO_InitStruct.Pin = FANS_PWR_CTRL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Push-Pull Output
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  // This call reassigns FANS_PWR_CTRL_PIN (from SWDIO to GPIO mode)
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	printf("The SWDIO contact is repurposed !\n");
}

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
	/* 0 - high priority (1-Wire timing is critical) ! */
	HAL_NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

	//do_test_timer();
	ds18b20_init_stage1();

  /* Start the timer base */
  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK) {
		printf("Error calling HAL_TIM_Base_Start_IT for TIM1 !!!\n");
	}

	ds18b20_init_stage2();
}

void fan2_RPM_callback(TIM_HandleTypeDef *htim) {
	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
		uint32_t now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

		if (last_capture != 0) {
			tach_period_us = (now >= last_capture)
				? (now - last_capture)
				: (0xFFFF - last_capture + now);
		}
		last_capture = now;
		capture_counter++;
	}
}

void MX_TIM3_Init(void)
{
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* Enable TIM3 clock */
	__HAL_RCC_TIM3_CLK_ENABLE();

	htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1919; // Sets frequency to 25kHz (48MHz / 1920)
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_PWM_Init(&htim3);
  HAL_TIM_IC_Init(&htim3);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = FAN2_PWM_START_VAL;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);

	if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) {
		printf("Error calling HAL_TIM_PWM_Start for TIM3->chan1 !!!\n");
	} else {
		printf("TIM3->chan1 PWM Start is OK\n");
	}
}

void MX_TIM14_Init(void)
{
	HAL_StatusTypeDef ret;
	TIM_IC_InitTypeDef sConfigIC = {0};

	/* Enable TIM14 clock */
	__HAL_RCC_TIM14_CLK_ENABLE();

	htim14.Instance = TIM14;
	htim14.Init.Prescaler = 0;
	//htim14.Init.Prescaler = (SystemCoreClock / 1000000) - 1; // 1 MHz timer
	htim14.Init.Prescaler = (SystemCoreClock / 10000) - 1; // 10 kHz timer
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 0xFFFF; // 65,535us max period (~65ms)
	htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init(&htim14);
  HAL_TIM_IC_Init(&htim14);

	// Configure Channel 1 for Input Capture on PA7
	sConfigIC.ICPolarity = TIM_ICPOLARITY_RISING;
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
	sConfigIC.ICFilter = 10; // Filter noise (adjust 0-15 as needed)
	HAL_TIM_IC_ConfigChannel(&htim14, &sConfigIC, TIM_CHANNEL_1);

	// Enable the interrupt for the timer
	/* 3 - lower priority (tachometer is non-critical) */
	HAL_NVIC_SetPriority(TIM14_IRQn, 3, 0);
	//HAL_NVIC_EnableIRQ(TIM14_IRQn);

	ret = HAL_TIM_RegisterCallback(&htim14, HAL_TIM_IC_CAPTURE_CB_ID, fan2_RPM_callback);
	if (ret != HAL_OK) {
		printf("HAL_TIM_RegisterCallback return error: %d !!!\n", ret);
	}

  if (HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1) != HAL_OK) {
		printf("Error calling HAL_TIM_IC_Start_IT for TIM14->chan1 !!!\n");
	}
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

void TIM14_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim14); // This is what triggers your fan2_RPM_callback
}
