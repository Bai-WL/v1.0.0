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
	.RstTransChan = bsp_485_rst_trans_chan};
BSP_ModbusHandleTypedef hMBWifiMaster;
BSP_ModbusOperationsTypedef hMBWifiOps = {
	.HardwareInit = bsp_atbm6431_init, /* TODO: 将开启AP热点封装进另一个函数中, 另外初始化需要返回成功状态 */
	.TimerInit = bsp_MBTIMInit,
	.SendMessage = bsp_atbm6431_send_message,
	.msg_fifo_get = bsp_atbm6431_get_message,
	.RstTransChan = bsp_atbm6431_rst_trans_chan};

void bsp_hhctl_init(void)
{
	nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); /* 4-bits for pre-emption priority */

	hid_init();
	bsp_SysTickInit();
	bsp_adc_init();

	pm_power_on();

	// bsp_RTC_Init();

	// bsp_drdusb_init();

	bsp_JLXLcdInit();
	bsp_JLXLcdClearScreen(0x00);

	/*WIFI复位引脚*/
	rcu_periph_clock_enable(RCU_GPIOB);
	gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
	gpio_bit_reset(GPIOB, GPIO_PIN_2);
}

void bsp_nvic_init(void)
{
}
