/**
 * @file bsp_i2c_gd32f30x.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_I2C_GD32F30X_H__
#define __BSP_I2C_GD32F30X_H__
#include "bsp_i2c.h"

#define GD32F30X_I2C0_2

#if (defined(GD32F30X_I2C0_1)) || (defined(GD32F30X_I2C0_2))
extern struct i2c_dev hi2c0; 
#endif

#endif /* __BSP_I2C_GD32F30X_H__ */
