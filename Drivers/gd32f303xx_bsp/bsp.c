/**
 * @file bsp.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.4
 * @date 2025-08-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x_gpio.h"
#include "bsp_gd32f30x_rtc.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_gd32f303xx_systick.h"
#include "drv_pm.h"
#include "drv_hid.h"

void bsp_RunPer1ms(void)
{
    bsp_RTCRunPer1ms();
    bsp_RefreshRealTime();
	hid_scan();
}

extern void bsp_scanVbusPer10ms(void);
void bsp_RunPer10ms(void)
{
    static uint32_t last_tick = 0;
	bsp_adc_run_per10ms();
    bsp_scanVbusPer10ms();
    pm_power_saver();

    if (last_tick != 0) {   /* ¹Ø»ú»òÐÝÃß */
        if (!gpio_input_bit_get(FUN_KEY_F2_PORT, FUN_KEY_F2_PIN)) {
            if ((bsp_get_systick() - last_tick) > 5000) {
                if (!gpio_input_bit_get(FUN_KEY_F5_PORT, FUN_KEY_F5_PIN)) {
                    last_tick = bsp_get_systick();
                    pm_suspend_request();
                }
                else if (!gpio_input_bit_get(FUN_KEY_F6_PORT, FUN_KEY_F6_PIN)) {
                    last_tick = bsp_get_systick();
                    pm_shutdown();
                }
            }
        } else {
            last_tick = bsp_get_systick();
        }
    } else {
        last_tick = bsp_get_systick();
    }
}
