/**
 * @file bsp_gd32f303xx_systick.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.4
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2024-2025
 * 
 */

#include "gd32f30x.h"
#include "bsp_gd32f30x_rtc.h"
#include "bsp.h"
#include "bsp_assert.h"

volatile uint32_t _ulSysDelay = 0 ;

void bsp_SysTickInit(void)
{
//    uint32_t sta;
//    /* setup systick timer for 1000Hz interrupts */
//    sta = SysTick_Config(SystemCoreClock / 1000);
//    bsp_assert(!sta);
    SysTick_Config(SystemCoreClock / 1000);

    /* configure the systick handler priority */
    NVIC_SetPriority(SysTick_IRQn, 0x00);
}

/**
 * @brief 系统时钟中断回调函数
 * 
 */
void bsp_SysTimDelayCallback(void)
{
	_ulSysDelay++;
	bsp_RunPer1ms();
	if (!(_ulSysDelay%10)) {
		bsp_RunPer10ms();
	}
}

uint32_t bsp_get_systick(void)
{
	return _ulSysDelay;
}

void bsp_delay(uint32_t ms){
	uint32_t start = bsp_get_systick();
	while((bsp_get_systick() - start) < ms);
}
