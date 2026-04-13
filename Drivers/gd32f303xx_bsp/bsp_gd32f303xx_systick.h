/**
 * @file bsp_gd32f303xx_systick.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef __BSP_GD32F303XX_SYSTICK_H__
#define __BSP_GD32F303XX_SYSTICK_H__
#include "stdint.h"

void bsp_SysTickInit(void) ;
void bsp_delay(uint32_t _ulcnt) ;
void bsp_SysTimDelayCallback(void) ;
uint32_t bsp_get_systick(void);

#endif /* __BSP_GD32F303XX_SYSTICK_H__ */
