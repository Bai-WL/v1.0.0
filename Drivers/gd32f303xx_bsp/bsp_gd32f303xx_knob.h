/**
 * @file bsp_gd32f303xx_knob.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-03-15
 * 
 * @copyright Copyright (c) 2024-2025
 * 
 */

#ifndef __BSP_GD32F303XX_KNOB_H__
#define __BSP_GD32F303XX_KNOB_H__

#include "stdint.h"

void bsp_knob_init(void);
int32_t bsp_getKnobCnt(void);
void bsp_KnobTimIRQCallback(void);
void bsp_KnobCntRunPer1ms(void);

#endif /* __BSP_GD32F303XX_KNOB_H__ */
