/**
 * @file bsp_gd32f30x_hhctl.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
#include "gd32f30x_gpio.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_gd32f303xx_mbboard.h"
#include "bsp_wifi.h"
#include "bsp_gd32f30x_rtc.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_drdusb.h"
#include "bsp_atbm6431.h"
#include "drv_pm.h"
#include "drv_hid.h"

BSP_ModbusHandleTypedef hMBMaster;
BSP_ModbusOperationsTypedef hMB485Ops = {
	.HardwareInit = bsp_MBHardInit, 
	.TimerInit = bsp_MBTIMInit,
	.SendMessage = bsp_MBDMASend_Polling,
	.RstTransChan = bsp_485_rst_trans_chan
};
BSP_ModbusHandleTypedef hMBWifiMaster;
BSP_ModbusOperationsTypedef hMBWifiOps = {
	.HardwareInit = bsp_atbm6431_init,	/* TODO: 将开启AP热点封装进另一个函数中, 另外初始化需要返回成功状态 */
	.TimerInit = bsp_MBTIMInit,
	.SendMessage = bsp_atbm6431_send_message,
	.msg_fifo_get = bsp_atbm6431_get_message,
	.RstTransChan = bsp_atbm6431_rst_trans_chan
};

void bsp_hhctl_init(void)
{
	char pbuf[32] = {0};
	uint32_t vBat3V7 = 0;
	uint32_t vBat3V = 0;
	
	nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);	/* 4-bits for pre-emption priority */

	hid_init();
	bsp_SysTickInit();
	bsp_adc_init();

	pm_power_on();
	
	// bsp_RTC_Init();
	
	//bsp_drdusb_init();

	bsp_JLXLcdInit();
	bsp_JLXLcdClearScreen(0x00);
	
	rcu_periph_clock_enable(RCU_GPIOB); 
	gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
	gpio_bit_reset(GPIOB, GPIO_PIN_2);
	while (hid_get_funkey() != 2) {
		vBat3V7 = bsp_readBat1Volt();
		vBat3V = bsp_readBat2Volt();
		sprintf(pbuf, "knob cnt: %5d", hid_get_knobcnt());
		bsp_JLXLcdShowString(8, 0, pbuf, 0, 0);
		sprintf(pbuf, "mtxkey num: %2d", hid_get_mtxkey());
		bsp_JLXLcdShowString(8, 2, pbuf, 0, 0);
		sprintf(pbuf, "Bat3v7: %03.3f=%d%%", vBat3V7/4095.0, bsp_readBat1VoltPercent());
		bsp_JLXLcdShowString(8, 4, pbuf, 0, 0);
		sprintf(pbuf, "Bat3v: %03.3f", vBat3V/4095.0);
		bsp_JLXLcdShowString(8, 6, pbuf, 0, 0);
		sprintf(pbuf, "CC1: %03.3f", bsp_GetCC1State()/4095.0);
		bsp_JLXLcdShowString(8, 8, pbuf, 0, 0);
		sprintf(pbuf, "CC2: %03.3f", bsp_GetCC2State()/4095.0);
		bsp_JLXLcdShowString(8, 10, pbuf, 0, 0);
		sprintf(pbuf, "IsInCharge%d", hdrdusb.vbus_valid);
		bsp_JLXLcdShowString(8, 12, pbuf, 0, 0);
		bsp_JLXLcdRefreshScreen();
	}
	bsp_JLXLcdClearScreen(0x00);
	bsp_JLXLcdShowString(8, 0, "WIFI Loading...", 0, 0);
	bsp_JLXLcdRefreshScreen();
	//bsp_ModbusInit(&hMBMaster, MB_MASTER, MB_RTU, 38400, &hMB485Ops);
	bsp_ModbusInit(&hMBWifiMaster, MB_MASTER, MB_TCP, 115200, &hMBWifiOps);
	bsp_JLXLcdClearScreen(0x00);
	bsp_JLXLcdShowString(8, 0, "WIFI Load Done!", 0, 0);
	bsp_JLXLcdRefreshScreen();
}

void bsp_nvic_init(void)
{
	
}
