#ifndef __TIM_H
#define __TIM_H
#include "stm32f0xx_hal.h"

void MX_TIM1_Init(void);
void MX_TIM3_Init(void);
void MX_TIM14_Init(void);
void ds18_tim_cb(TIM_HandleTypeDef *htim);
void ds18_tim_test_cb(TIM_HandleTypeDef *htim);
void delay_ms(uint32_t ms);

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim14;

#endif /* __TIM_H */
