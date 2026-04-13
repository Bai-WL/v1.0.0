/**
 * @file bsp_gd32f303xx_mbboard.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-07
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F303XX_MBBOARD_H__
#define __BSP_GD32F303XX_MBBOARD_H__

#include "bsp_types.h"

#define MBTIM TIMER4
#define MBTIM_DISABLE()  timer_disable(MBTIM)
#define MBTIM_ENABLE()   timer_enable(MBTIM)
#define MBTIM_LOAD_RST() timer_counter_value_config(MBTIM, 0)

#define MB_RxState() gpio_bit_reset(GPIOB, GPIO_PIN_12)
#define MB_TxState() gpio_bit_set(GPIOB, GPIO_PIN_12)

void bsp_MBTIMInit(void); 
void bsp_MBHardInit(uint32_t _ulBaud);
BSP_StatusTypedef bsp_MBDMASend_Polling(const uint8_t *_pcBuf, uint32_t _usLen);
void bsp_485_rst_trans_chan(void);

#endif /* __BSP_GD32F303XX_MBBOARD_H__ */
