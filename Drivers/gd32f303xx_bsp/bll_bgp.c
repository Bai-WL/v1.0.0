/**
 * @file bll_bgp.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-08-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */ 
#include "bll_bgp.h"
#include "drv_pm.h"
#include "drv_hid.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_drdusb.h"

static void draw_popup_window(char* str);

/**
 * @brief basic logic layer background power manage drawing process, force display, seprated from app process.
 * 
 */
void bg_process(void)
{
    static uint32_t last_tick_0 = 0;
    static uint32_t last_tick_1 = 0;
	static uint32_t last_tick_2 = 0;
    static uint8_t lp_warn = 1;
    static uint8_t sta = 0;
	static uint8_t sd_flg = 0;

	if (!last_tick_0)
		last_tick_0 = bsp_get_systick();
	if (!last_tick_1)
		last_tick_1 = bsp_get_systick();
	if (!last_tick_2)
		last_tick_2 = bsp_get_systick();
	
	if (sta != 1) {
		if (hid_get_funkey() == 1) {    
			if ((bsp_get_systick() - last_tick_0) > 3000) { /* keep F1 pressed in 3s */
				sta = 1;
			}
		} else {
			last_tick_0 = bsp_get_systick();
		}
	}
	
    switch (sta) {
    case 0:
        if (pm_get_req()) {
            pm_handler();   /* pm handler is a blocked-process */
            last_tick_0 = bsp_get_systick();  /* compensate the period of suspend for long press detection */
            return;     /* enter pm handle sequence, force return */
        }

        if (pm_get_lpsta()) {
            sta = 2;
            last_tick_2 = bsp_get_systick() - 1000;
            break;
        }

        if (bsp_readBat1VoltPercent() <= 10) {  /* software smit filter */
            if (!hdrdusb.vbus_valid && lp_warn) {
                draw_popup_window("电量低, 请充电");
                bsp_JLXLcdShowString(76, 5, "电量低", 0, 0);
                bsp_JLXLcdShowString(76, 8, "请充电", 0, 0);
                if (hid_get_funkey() && (bsp_get_systick() - last_tick_1) > 1000) { /* show at least 1s */
                    lp_warn = 0;
                }
            }
        } else if (!lp_warn && bsp_readBat1VoltPercent() >= 30) {
            lp_warn = 1;
        } else if (lp_warn) {
            last_tick_1 = bsp_get_systick();
        } else {
//            __NOP();
        }
        break;
    case 1:
        if (sd_flg || !hid_get_funkey()) {
			sd_flg = 1;
            draw_popup_window("关机中...");
            bsp_JLXLcdShowString(72, 5, "关机中...", 0, 0);
            if ((bsp_get_systick() - last_tick_0) > 5000) {
                pm_shutdown();
            }
        } else {
            draw_popup_window("准备关机!!请松开按键");
            bsp_JLXLcdShowString(66, 5, "准备关机!!", 0, 0);
            bsp_JLXLcdShowString(64, 8, "请松开按键", 0, 0);
            last_tick_0 = bsp_get_systick();
        }
        break;
    case 2:
        if (hdrdusb.vbus_valid) {   /* device is charging, reset sta */
            sta = 0;
        } else {
            if (!hid_get_funkey() && !hid_get_mtxkey()) {
                if (bsp_get_systick() - last_tick_2 > 500) {
                    draw_popup_window("电量耗尽，即将关机");
                    bsp_JLXLcdShowString(64, 5, "电量耗尽!!", 0, 0);
                    bsp_JLXLcdShowString(70, 8, "即将关机", 0, 0);
                }
            } else {
                last_tick_2 = bsp_get_systick();
            }
        }
        break;
    default:
        sta = 0;
        break;
    }
}

/**
 * @brief draw a fixed size popup window
 * 
 */
static void draw_popup_window(char* str)
{
    // /* clean popup window area */
    // bsp_JLXLcdClearRect( 56, 80,  3, 10, 0x00);    
    // /* left line */
    // bsp_JLXLcdClearRect( 57,  2,  3, 10, 0xFF);    
    // /* right line */
    // bsp_JLXLcdClearRect(134,  2,  3, 10, 0xFF);    
    // /* top line */
    // bsp_JLXLcdClearRect( 57, 78,  3,  1, 0x60);     
    // bsp_JLXLcdClearRect( 57,  2,  3,  1, 0x7F);     
    // bsp_JLXLcdClearRect(134,  2,  3,  1, 0x7F);     
    // /* bot line */
    // bsp_JLXLcdClearRect( 57, 78, 12,  1, 0x06);    
    // bsp_JLXLcdClearRect( 57,  2, 12,  1, 0xFE);     
    // bsp_JLXLcdClearRect(134,  2, 12,  1, 0xFE);     

    /* clean popup window area */
    bsp_JLXLcdClearRect( 40, 113,  3, 10, 0x00);    
    /* left line */
    bsp_JLXLcdClearRect( 41,  2,  3, 10, 0xFF);    
    /* right line */
    bsp_JLXLcdClearRect(150,  2,  3, 10, 0xFF);    
    /* top line */
    bsp_JLXLcdClearRect( 41, 110,  3,  1, 0x60);     
    bsp_JLXLcdClearRect( 41,  2,  3,  1, 0x7F);     
    bsp_JLXLcdClearRect(150,  2,  3,  1, 0x7F);     
    /* bot line */
    bsp_JLXLcdClearRect( 41, 110, 12,  1, 0x06);    
    bsp_JLXLcdClearRect( 41,  2, 12,  1, 0xFE);     
    bsp_JLXLcdClearRect(150,  2, 12,  1, 0xFE); 

	/* for align test */
//	bsp_JLXLcdClearRect(95, 2, 0, 16, 0xFF);
//	bsp_JLXLcdClearRect(0, 192, 7, 1, 0x01);
//	bsp_JLXLcdClearRect(0, 192, 8, 1, 0x80);

    (void)str;
}
