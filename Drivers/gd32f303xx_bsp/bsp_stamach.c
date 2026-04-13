/**
 * @file bsp_stamach.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "bsp_stamach.h"

uint32_t sm_list[32] = {0};
uint32_t sm_sta[32]  = {0};
struct sta_mach sm = {0};

void bsp_sm_init(void)
{
    rcu_periph_clock_enable(RCU_AF);
    nvic_irq_enable(EXTI0_IRQn, 2, 0);  /* config software interrupt exti0 */
    exti_interrupt_enable(EXTI_0);
}

uint32_t sta_mach_register(void* dev)
{
    uint32_t i = 0;
    uint32_t chnl = 32;
    /* find whether the request device is registered */
    while (i < 32 && (sm.dev_list[i] != dev)) {
        if (chnl == 32 && !sm.sm_list[i]) 
            chnl = i;
        i++;
    }
    if (i != 32 || chnl == 32) 
        goto _RET;
	sm.dev_list[chnl] = dev;
    sm.sm_list[chnl] = 1;
    sm_clear_itflag(chnl);
    sm_clear_done(chnl);

    return chnl;
    _RET:
        return 0XFFFFFFFF;
}

void sta_mach_start(uint32_t sm_desc)
{
    while (sm_get_busy(sm_desc));
    sm.sm_sta[sm_desc] = 1;
    sm_clear_done(sm_desc); /* no matter if users check done flag */
    sm_set_itflag(sm_desc);
}

/**
 * @brief unregister the descriptor in state machine list
 * 
 * @param sm_desc 
 */
void sta_mach_unregister(uint32_t sm_desc)
{
    sm.sm_list[sm_desc] = 0;
    sm_clear_itflag(sm_desc);
    sm_clear_done(sm_desc);
}

void swi_isr(void)
{
    volatile uint8_t i = 0;
    BSP_StatusTypedef bsp_sta;
    exti_interrupt_flag_clear(EXTI_0);
    while (i < 32) {
        if (sm_get_itflag(i) && sm.sm_sta[i]) {  /* if state machine is working */ /* TODO:  */
            sm_set_busy(i);
            if ((bsp_sta = sm.sm_call[i](sm.sm_sta[i])) == BSP_DEFAULT) {
                sm.sm_sta[i]++;
            } else if (bsp_sta == BSP_OK) {
                sm_clear_busy(i);
                sm_set_done(i);
                sm.sm_sta[i] = 0;
                if (bsp_sta == BSP_ERROR) {
                    ;   /* TODO: do something to handle error */
                }
            }
            sm_clear_itflag(i);
        }
        i++;
    }
}
