/**
 * @file bsp_gd32f30x_hhctl.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F30X_HHCTL_H__
#define __BSP_GD32F30X_HHCTL_H__

#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f303xx_knob.h"
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_gd32f303xx_mtxkey_poll.h"
#include "bsp_gd32f30x_hhctl_funkey.h"
#include "bsp_gd32f303xx_mbboard.h"
#include "bsp_gd32f30x_rtc.h"

extern BSP_ModbusHandleTypedef hMBMaster;
extern BSP_ModbusHandleTypedef hMBWifiMaster;
void bsp_hhctl_init(void);

#endif  /* __BSP_GD32F30X_HHCTL_H__ */
