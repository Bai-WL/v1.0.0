/**
 * @file bsp_gd32f303xx_mtxkey_poll.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-3-11
 * 
 * @copyright Copyright (c) 2024-2025
 * 
 */
#ifndef __BSP_GD32F303XX_MTXKEY_POLL_H__
#define __BSP_GD32F303XX_MTXKEY_POLL_H__
#include "stdint.h"

#define RCU_MTX_KEY_X1 RCU_GPIOC
#define RCU_MTX_KEY_X2 RCU_GPIOC
#define RCU_MTX_KEY_X3 RCU_GPIOC
#define RCU_MTX_KEY_X4 RCU_GPIOA

#define MTX_KEY_X1_PORT GPIOC
#define MTX_KEY_X2_PORT GPIOC
#define MTX_KEY_X3_PORT GPIOC
#define MTX_KEY_X4_PORT GPIOA

#define MTX_KEY_X1_PIN GPIO_PIN_12
#define MTX_KEY_X2_PIN GPIO_PIN_11
#define MTX_KEY_X3_PIN GPIO_PIN_10
#define MTX_KEY_X4_PIN GPIO_PIN_15

#define RCU_MTX_KEY_Y1 RCU_GPIOB
#define RCU_MTX_KEY_Y2 RCU_GPIOC
#define RCU_MTX_KEY_Y3 RCU_GPIOC
#define RCU_MTX_KEY_Y4 RCU_GPIOC

#define MTX_KEY_Y1_PORT GPIOB
#define MTX_KEY_Y2_PORT GPIOC
#define MTX_KEY_Y3_PORT GPIOC
#define MTX_KEY_Y4_PORT GPIOC

#define MTX_KEY_Y1_PIN GPIO_PIN_5
#define MTX_KEY_Y2_PIN GPIO_PIN_1 
#define MTX_KEY_Y3_PIN GPIO_PIN_2 
#define MTX_KEY_Y4_PIN GPIO_PIN_3 

void bsp_MtxKeyPollInit(void);
uint8_t bsp_ucScanMtxKey(void);

#endif /* __BSP_GD32F303XX_MTXKEY_POLL_H__ */
