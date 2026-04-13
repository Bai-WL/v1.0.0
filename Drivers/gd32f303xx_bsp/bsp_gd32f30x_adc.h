/**
 * @file bsp_gd32f30x_adc.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-03-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F30X_ADC_H__
#define __BSP_GD32F30X_ADC_H__
#include "stdint.h"
void bsp_adc_init(void);
uint32_t bsp_readBat1Volt(void);
uint32_t bsp_readBat2Volt(void);
uint32_t bsp_GetCC1State(void);
uint32_t bsp_GetCC2State(void);
uint8_t bsp_readBat1VoltPercent(void);
void bsp_adc_run_per10ms(void);
#endif /* __BSP_GD32F30X_ADC_H__ */
