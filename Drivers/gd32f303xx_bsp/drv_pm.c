/**
 * @file drv_pm.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.10
 * @date 2025-08-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <stdint.h>

#include "gd32f30x_rcu.h"
#include "gd32f30x_pmu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_misc.h"
#include "gd32f30x_exti.h"

#include "drv_pm.h"
#include "drv_hid.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_atbm6431.h"
#include "bsp_modbus_tcp.h"
#include "bsp_atbm6431.h"
#include "bsp_i2c_gd32f30x.h"
#include "bsp_wifi_trans.h"
#include "bsp_drdusb.h"

extern BSP_ModbusHandleTypedef hMBWifiMaster;
extern BSP_ModbusOperationsTypedef hMBWifiOps;
extern BSP_ModbusHandleTypedef hMBMaster;
extern BSP_ModbusOperationsTypedef hMB485Ops;

#define pwr_enable() gpio_bit_set(GPIOA, GPIO_PIN_1);
#define pwr_disable() gpio_bit_reset(GPIOA, GPIO_PIN_1);

static uint8_t dont_sleep = 0;
static uint8_t pm_req = 0;

void draw_battery(uint8_t power)
{
    /* top line */
    bsp_JLXLcdClearRect(72, 48, 4, 1, 0x06);
    bsp_JLXLcdClearRect(72, 2, 4, 1, 0x07);
    bsp_JLXLcdClearRect(118, 2, 4, 1, 0x07);

    bsp_JLXLcdClearRect(86, 20, 4, 1, 0xC0);
    bsp_JLXLcdClearRect(86, 2, 4, 1, 0xFE);
    bsp_JLXLcdClearRect(106, 2, 4, 1, 0xFE);

    /* left border */
    bsp_JLXLcdClearRect(72, 2, 5, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 6, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 7, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 8, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 9, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 10, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 11, 1, 0xFF);
    bsp_JLXLcdClearRect(72, 2, 12, 1, 0xFF);

    /* right border */
    bsp_JLXLcdClearRect(118, 2, 5, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 6, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 7, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 8, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 9, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 10, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 11, 1, 0xFF);
    bsp_JLXLcdClearRect(118, 2, 12, 1, 0xFF);

    /* botton line */
    bsp_JLXLcdClearRect(72, 48, 13, 1, 0x60);
    bsp_JLXLcdClearRect(72, 2, 13, 1, 0xE0);
    bsp_JLXLcdClearRect(118, 2, 13, 1, 0xE0);

    switch (power)
    {
    case 4:
        bsp_JLXLcdClearRect(76, 40, 5, 1, 0x7F);
        bsp_JLXLcdClearRect(76, 40, 6, 1, 0xFE);
    case 3:
        bsp_JLXLcdClearRect(76, 40, 7, 1, 0x7F);
        bsp_JLXLcdClearRect(76, 40, 8, 1, 0xFE);
    case 2:
        bsp_JLXLcdClearRect(76, 40, 9, 1, 0x7F);
        bsp_JLXLcdClearRect(76, 40, 10, 1, 0xFE);
    case 1:
        bsp_JLXLcdClearRect(76, 40, 11, 1, 0x7F);
        bsp_JLXLcdClearRect(76, 40, 12, 1, 0xFE);
    default:
        break;
    }
}

/**
 * @brief Put this function at the beginning of main function
 *
 */
void pm_power_on(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); /* 4-bits for pre-emption priority */

#ifdef WIFI_H02W
    uint8_t blink_flg = 0;
    uint8_t power = 0;
    uint8_t pbat = 0;
    uint32_t last_tick = 0;
    /* WiFi版本：复位WiFi设备引脚 */
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
    gpio_bit_reset(GPIOB, GPIO_PIN_2); /* keep reset wifi device */
#endif

    hid_init();
    bsp_SysTickInit();
    bsp_adc_init();
#if defined(GD32F30X_CL)
    bsp_drdusb_init();
#elif defined(GD32F30X_HD)
    bsp_usbd_init();
#endif

#ifdef WIFI_H02W
    /* WiFi版本：检测USB充电，显示充电页面 */
    while (bsp_get_systick() < 1000)
        ;

    while (bsp_get_systick() < 2000)
    {
        if (hdrdusb.vbus_valid)
        {
            rcu_periph_clock_enable(RCU_GPIOA);
            gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_1);
            pwr_enable();
            bsp_JLXLcdInit();
            goto _CHARGING_PAGE;
        }
    }
#endif

    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_1);
    pwr_enable();

    goto _DEVICE_INIT;
#ifdef WIFI_H02W
_CHARGING_PAGE:
    while ((!hid_get_funkey() && !hid_get_mtxkey()))
    {
        if (!hdrdusb.vbus_valid)
        {
            pm_shutdown();
        }
        bsp_JLXLcdClearScreen(0x00);

        if ((bsp_get_systick() - last_tick) > 1000)
        {
            if (blink_flg)
            {
                blink_flg = 0;
            }
            else
            {
                blink_flg = 1;
            }
            pbat = bsp_readBat1VoltPercent();
            if (pbat < 25)
            {
                power = 1;
            }
            else if (pbat < 50)
            {
                power = 2;
            }
            else if (pbat < 75)
            {
                power = 3;
            }
            else if (pbat < 100)
            {
                power = 4;
            }
            else
            {
                power = 4;
                blink_flg = 0; /* prohibit blink */
            }
            last_tick = bsp_get_systick();
        }
        if (blink_flg)
        {
            draw_battery(power - 1);
        }
        else
        {
            draw_battery(power);
        }
        bsp_JLXLcdRefreshScreen();
    }
    bsp_JLXLcdClearScreen(0xFF);
    bsp_JLXLcdRefreshScreen();
// bsp_JLXLcdEnterSleepMode();
#endif
_DEVICE_INIT:
    bsp_JLXLcdInit();
    bsp_JLXLcdClearScreen(0x00);
    bsp_JLXLcdRefreshScreen();
    // bsp_RTC_Init();

#ifdef WIFI_H02W
    bsp_modbus_tcp_window_init();
    bsp_ModbusInit(&hMBWifiMaster, MB_MASTER, MB_TCP, 115200, &hMBWifiOps);
#else
    // 有线版本初始化RS485 Modbus
    bsp_ModbusInit(&hMBMaster, MB_MASTER, MB_RTU, 38400, &hMB485Ops);
#endif
}

void pm_dont_sleep(void)
{
    dont_sleep = 1;
}

/* TODO: 在HardFault中执行睡眠函数 */
/**
 * @brief 注意，为使睡眠禁止，需要循环调用按键扫描函数，重置睡眠标志。
 *
 *
 */
#define SHUTDOWN_PERIOD 900000 /* 15 minutes */
#define SUSPEND_PERIOD 300000  /* 5 minutes */
static void pm_sleep_scan(void)
{
    static uint32_t last_tick = 0;

    if (dont_sleep == 0)
    {
        if (last_tick == 0)
            last_tick = bsp_get_systick();

        if (!hdrdusb.vbus_valid && (bsp_get_systick() - last_tick) > SHUTDOWN_PERIOD)
        {
            pm_shutdown();
        }
        else if ((bsp_get_systick() - last_tick) > SUSPEND_PERIOD)
        {
            pm_suspend_request();
        }
    }
    else
    {
        pm_req = 0; /* reset pm request */
        dont_sleep = 0;
        last_tick = bsp_get_systick();
    }
}

/**
 * @brief Put it in an periodic interrupt function which has a period of 10ms
 *
 */
static uint8_t lp_shutdown = 0;
void pm_power_saver(void)
{

#ifdef WIFI_H02W
    static uint32_t last_tick = 0;
    /* WiFi版本：电池电量检测 */
    uint32_t BatVolt = bsp_readBat1Volt();
    uint32_t BatVoltPercent = bsp_readBat1VoltPercent();

    if (!lp_shutdown)
    {
        if (!hdrdusb.vbus_valid && BatVolt && BatVoltPercent < 5)
        { /* bat1 < 3.1V */
            lp_shutdown = 1;
            last_tick = bsp_get_systick();
            return;
        }
        pm_sleep_scan();
    }
    else
    {                           /* forced shutdown */
        if (hdrdusb.vbus_valid) /* abort shutdown */
            lp_shutdown = 0;
        else if ((bsp_get_systick() - last_tick) > 60000)
            pm_shutdown();
    }
#else
    /* 有线版本：无电池电量检测，仅进行睡眠扫描 */
    if (!lp_shutdown)
    {
        pm_sleep_scan();
    }
    else
    {
        /* 有线版本不会因为低电量进入强制关机 */
        lp_shutdown = 0;
    }
#endif
}

uint8_t pm_get_lpsta(void)
{
    return lp_shutdown;
}

uint8_t pm_get_req(void)
{
    return pm_req;
}

void pm_suspend_request(void)
{
    pm_req = 1;
}

void pm_shutdown_request(void)
{
    pm_req = 2;
}

/**
 * @brief stop lots of commn
 *
 */
static void do_sth_before_suspend(void)
{
    hi2c0.i2c_ops.rst(&hi2c0);
    bsp_modbus_tcp_reset_windows();
    bsp_atbm6431_enter_sleepmode();
    bsp_JLXLcdEnterSleepMode();
}

static void pm_suspend(void)
{
    uint8_t fkeyVal = 0;
    uint8_t mkeyVal = 0;
    uint8_t lastFkeyVal = 0;
    uint8_t lastMkeyVal = 0;

    do_sth_before_suspend();

    lastFkeyVal = hid_get_funkey();
    lastMkeyVal = hid_get_mtxkey();

    while (1)
    {
        fkeyVal = hid_get_funkey();
        mkeyVal = hid_get_mtxkey();

        if ((!lastFkeyVal && fkeyVal) || (!lastMkeyVal && mkeyVal)) /* capture the falling edge */
            break;

        lastFkeyVal = fkeyVal;
        lastMkeyVal = mkeyVal;
    }
}

/**
 * @brief
 *
 */
static void do_sth_in_resume(void)
{
    bsp_atbm6431_exit_sleepmode();
    bsp_JLXLcdExitSleepMode();
}

static void pm_resume(void)
{
    do_sth_in_resume();
}

void pm_shutdown(void)
{
    bsp_JLXLcdEnterSleepMode();
    pwr_disable();
    __set_FAULTMASK(1);
    NVIC_SystemReset();
}

/**
 * @brief Put this function in main while to hanle suspend event
 *
 */
/* DEBUG: 20250728 test resume speed */
uint8_t resume_flag = 0;
uint32_t r_tick = 0;
/* DEBUG: 20250728 test resume speed */
void pm_handler(void)
{
    uint32_t mutex = 0;
    if (!pm_req)
        return;

    mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
    if (!mutex)
        return;
    bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);

    switch (pm_req)
    {
    case 1:
        pm_suspend();
        /* DEBUG: 20250728 test resume speed */
        resume_flag = 1;
        r_tick = bsp_get_systick();
        /* DEBUG: 20250728 test resume speed */
        pm_resume();
        break;
    case 2:
        pm_shutdown();
        break;
    default:
        pm_shutdown();
        break;
    }

    pm_req = 0;
}
