/**
 * @file bsp_gd32f30x_rcu.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-07-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F30X_RCU_H__
#define __BSP_GD32F30X_RCU_H__
#include "stdint.h"

enum bsp_sys_clock_value {
    SYS_CLOCK_120M = 0,
    SYS_CLOCK_108M,
    SYS_CLOCK_72M,
    SYS_CLOCK_48M,
    SYS_CLOCK_8M
};

void bsp_reinit_rcu(enum bsp_sys_clock_value sys_clock_value);

#endif /* __BSP_GD32F30X_RCU_H__ */
