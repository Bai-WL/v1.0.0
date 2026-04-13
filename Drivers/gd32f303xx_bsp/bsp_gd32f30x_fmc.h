/**
 * @file bsp_gd32f30x_fmc.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_GD32F30X_FMC_H__
#define __BSP_GD32F30X_FMC_H__
#include "stdint.h"
/* 128KB Flash */
#define FMC_PAGE_SIZE ((uint16_t)0x800U)    /* 2KB */
#define FMC_WRITE_START_ADDR ((uint32_t)0x0801F800U)
#define FMC_WRITE_END_ADDR ((uint32_t)0x0801FFFFU)

void bsp_fmc_erase_page(void);
void bsp_fmc_program(const uint8_t* data, uint32_t len, uint32_t addr);
uint32_t bsp_get_fmc_start_addr(void);
#endif /* __BSP_GD32F30X_FMC_H__ */
