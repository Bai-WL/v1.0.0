/**
 * @file bsp_gd32f303xx_mbuser.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-03-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F303XX_MBFUNCTION_H__
#define __BSP_GD32F303XX_MBFUNCTION_H__

#define MODBUS_SLAVE_ADDR 0x01

#define MODBUS_FUNCTIONS_COUNTS 5   /* TODO: Optimize this to an auto-calculate value */
#define MODBUS_BASEADDR_COUNTS 3

#define MODBUS_RSP_TIMEOUT 150 // 响应超时时间，单位ms, 必须为10ms的倍数

#endif /* __BSP_GD32F303XX_MBFUNCTION_H__ */
