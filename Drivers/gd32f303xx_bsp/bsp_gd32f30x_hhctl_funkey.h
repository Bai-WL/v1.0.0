/**
 * @file bsp_gd32f30x_hhctl_funkey.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2025-06-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __BSP_GD32F30X_HHCTL_FUNKEY_H__
#define __BSP_GD32F30X_HHCTL_FUNKEY_H__

#include "stdint.h"

#define RCU_FUN_KEY_F1 RCU_GPIOC
#define RCU_FUN_KEY_F2 RCU_GPIOC
#define RCU_FUN_KEY_F3 RCU_GPIOC
#define RCU_FUN_KEY_F4 RCU_GPIOB
#define RCU_FUN_KEY_F5 RCU_GPIOB
#define RCU_FUN_KEY_F6 RCU_GPIOD

#define FUN_KEY_F1_PORT GPIOC
#define FUN_KEY_F2_PORT GPIOC
#define FUN_KEY_F3_PORT GPIOC
#define FUN_KEY_F4_PORT GPIOB
#define FUN_KEY_F5_PORT GPIOB
#define FUN_KEY_F6_PORT GPIOD

#define FUN_KEY_F1_PIN GPIO_PIN_15
#define FUN_KEY_F2_PIN GPIO_PIN_14
#define FUN_KEY_F3_PIN GPIO_PIN_13
#define FUN_KEY_F4_PIN GPIO_PIN_4 
#define FUN_KEY_F5_PIN GPIO_PIN_3 
#define FUN_KEY_F6_PIN GPIO_PIN_2 

void bsp_FunKeyInit(void);
uint8_t bsp_ucScanFunKey(void);

#endif  /* __BSP_GD32F30X_HHCTL_FUNKEY_H__ */
