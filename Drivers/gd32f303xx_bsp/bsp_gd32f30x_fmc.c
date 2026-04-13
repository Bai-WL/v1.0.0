/**
 * @file bsp_gd32f30x_fmc.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "bsp_gd32f30x_fmc.h"
#include "gd32f30x_fmc.h"
#include "stdint.h"
#include "bsp_assert.h"
void bsp_fmc_erase_page(void)
{
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    fmc_page_erase(FMC_WRITE_START_ADDR);
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    fmc_lock();
}

/**
 * @brief 
 * 
 * @param data 
 * @param len 
 * @param addr 传入地址必须为4的倍数 
 */
void bsp_fmc_program(const uint8_t* data, uint32_t len, uint32_t addr)
{
    uint32_t i;
    uint32_t *pul = (uint32_t *)data;
	
    bsp_assert(!(addr%4));
    bsp_assert(addr >= FMC_WRITE_START_ADDR);
	bsp_assert(addr <= FMC_WRITE_END_ADDR);

    fmc_unlock();

    for (i = 0; i < len/4; i++) {
		if (addr + i*4 + 4 > FMC_WRITE_END_ADDR + 1) {
			fmc_lock();
			return ;
		}
        fmc_word_program(addr + i*4, *(pul+i));
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    }
    if (len % 4) {
        fmc_word_program(addr + i*4, *(pul+i));
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    }

    fmc_lock();
}

/**
 * @brief 
 * 
 * @return uint32_t 
 */
uint32_t bsp_get_fmc_start_addr(void)
{
    return FMC_WRITE_START_ADDR;
}
