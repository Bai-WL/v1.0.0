/**
 * @file drv_hid.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-29
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __DRV_HID_H__
#define __DRV_HID_H__

#include "stdint.h"

/***************************** Function Key *****************************/
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


/***************************** Matrix Key *****************************/
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

void hid_init(void);
void hid_scan(void);
uint8_t hid_get_funkey(void);
uint8_t hid_get_mtxkey(void);
uint32_t hid_get_knobcnt(void);

#endif /* __DEV_HID_H__ */
