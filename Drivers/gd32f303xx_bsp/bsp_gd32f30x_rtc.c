/**
 * @file bsp_gd32f30x_rtc.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-01-18
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "gd32f30x_misc.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_exti.h"
#include "gd32f30x_gpio.h"
#include "bsp_i2c.h"
#include "bsp_i2c_gd32f30x.h"
#include "bsp_gd32f30x_rtc.h"
#include "bsp_gd32f303xx_systick.h"

uint8_t sec_int_flag = 0;
uint8_t i2c_buf[7] = {0};
static struct realtime_t g_hrt1 = {0};
static struct realtime_t g_hrt2 = {0};
static struct realtime_t *_pinthrt = &g_hrt1;

static BSP_StatusTypedef eConfigRTCSecondInterrupt(void);

void bsp_RTC_Init(void)
{
    /* i2c hardware init */
    hi2c0.i2c_ops.init(&hi2c0, I2C_MASTER);

    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AF);
    /* enable sd8810 second interrupt */
    while (eConfigRTCSecondInterrupt());
    /* get real time once */
	sec_int_flag = 1;
    while (!_pinthrt->valid); 
	// while ( bsp_RefreshRealTime());
	// hi2c0.busy = 1;
}

/**
 * @brief 实时时间获取函数
 *        注：不要在任何中断中使用
 *        对于实时数据采用双缓存以避免数据在被读取时受到更改，指针的切换由用户进行，可以确保指针切换时间点的合法性，切换时间点为需要读取RTC数据的时候。
 *        此方法可确保数据完整性以及数据的连续性，但会造成数据的延迟。
 *        另加一数据有效标志，用以在用户读取数据时，若时钟未更新，放弃指针切换，以确保数据正确性。 
 *  
 * @return realtime_t* 
 */
struct realtime_t* bsp_RTCGetTime(void) 
{
    if (_pinthrt == &g_hrt1) {
        if (_pinthrt->valid) {
            g_hrt2.valid = 0;
            _pinthrt = &g_hrt2;
            return &g_hrt1;
        }
        return &g_hrt2;
    } else {
        if (_pinthrt->valid) {
            g_hrt1.valid = 0;
            _pinthrt = &g_hrt1;
            return &g_hrt2;
        }
        return &g_hrt1;
    }
}

/**
 * @brief 
 * 
 * @param data [second, minute, day, hour, month, year]
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_RTCSetRealTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    static enum {
        IS_I2C_BUSY = 0,
        DO_I2C_WRITE
    } state = IS_I2C_BUSY;
    BSP_StatusTypedef bsp_sta = BSP_BUSY;

    switch (state) {
    case IS_I2C_BUSY:
        __disable_irq();
        if (!hi2c0.busy) {
            hi2c0.busy = 1; /* 加锁为原子操作 */
            state = DO_I2C_WRITE;
            __enable_irq();
            i2c_buf[0] = num2bcd(second);   /* second */
            i2c_buf[1] = num2bcd(minute);   /* minute */
            i2c_buf[2] = num2bcd(hour);     /* hour */
            i2c_buf[4] = num2bcd(day);      /* day */
            i2c_buf[5] = num2bcd(month);    /* month */
            i2c_buf[6] = num2bcd((uint8_t)(year - 2000)); /* year */
            /* switch date to week */
            month = ((month+9)%12+3);
            year  = ((month-12) > 0) ? (year - 1) : year;
            i2c_buf[3] = 1 << ((day + 2*month + 3*(month+1)/5 + year + year/4 - year/100 + year/400) % 7); /* week */
        } else {
            __enable_irq();
            break;
        }
    case DO_I2C_WRITE:
        bsp_sta = hi2c0.i2c_ops.write(&hi2c0, 0x10, i2c_buf, 7);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR) 
                goto _RET_ERROR;
            hi2c0.busy = 0;
            state = IS_I2C_BUSY;
            return BSP_OK;
        }
        break;
    default:
        break;
    }

    return BSP_BUSY;
_RET_ERROR:
    hi2c0.busy = 0;
    state = IS_I2C_BUSY;
    return BSP_ERROR;
}

/**
 * @brief  每500ms扫描一次 
 * 
 * @return BSP_StatusTypedef 
 */
void bsp_RTCRunPer1ms(void)
{
    static uint32_t last_tick = 0;
    if ((bsp_get_systick() - last_tick) > 500) {
        sec_int_flag = 1;
        last_tick += 500; 
    }
}

/**
 * @brief run this function in bsp_RunPer1ms()
 * 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_RefreshRealTime(void)
{
    static enum {
        IS_SECINT_COME = 0,
        IS_I2C_BUSY,
        DO_I2C_READ
    } state = IS_SECINT_COME;
    BSP_StatusTypedef bsp_sta = BSP_BUSY;

    switch (state) {
    case IS_SECINT_COME:
        if (sec_int_flag) {
            state = IS_I2C_BUSY;
            sec_int_flag = 0;
        } else {
            break;
        }
    case IS_I2C_BUSY:
        __disable_irq();
        if (!hi2c0.busy) {
            hi2c0.busy = 1; /* 加锁为原子操作 */
            state = DO_I2C_READ;
            __enable_irq();
        } else {
            __enable_irq();
            break;
        }
    case DO_I2C_READ:
        bsp_sta = hi2c0.i2c_ops.read(&hi2c0, 0x10, i2c_buf, 7);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR) 
                goto _RET_ERROR;
            hi2c0.busy = 0;
            _pinthrt->second = bcd2num(i2c_buf[0]);
            _pinthrt->minute = bcd2num(i2c_buf[1]);
            _pinthrt->hour   = bcd2num(i2c_buf[2]);
            _pinthrt->week   = bcd2num(i2c_buf[3]);
            _pinthrt->day    = bcd2num(i2c_buf[4]);
            _pinthrt->month  = bcd2num(i2c_buf[5]);
            _pinthrt->year   = bcd2num(i2c_buf[6]) + 2000;
            _pinthrt->valid = 1;
            state = IS_SECINT_COME;
            return BSP_OK;
        }
        break;
    default:
        break;
    }

    return BSP_BUSY;
_RET_ERROR:
    hi2c0.busy = 0;
    state = IS_SECINT_COME;
    return BSP_ERROR;
}

static BSP_StatusTypedef eConfigRTCSecondInterrupt(void)
{
    static enum {
        IS_I2C_BUSY,
        DO_I2C_READ_1,
        DO_I2C_WRITE_1,
        DO_I2C_WRITE_2
    } state = IS_I2C_BUSY;
    BSP_StatusTypedef bsp_sta = BSP_BUSY;

    switch (state) {
    case IS_I2C_BUSY:
        __disable_irq();
        if (!hi2c0.busy) {
            hi2c0.busy = 1;
            state = DO_I2C_READ_1;
            __enable_irq();
        } else {
            __enable_irq();
            break;
        }
    case DO_I2C_READ_1:
        bsp_sta = hi2c0.i2c_ops.read(&hi2c0, 0x1D, i2c_buf, 3);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR) 
                goto _RET_ERROR;
            state = DO_I2C_WRITE_1;
            i2c_buf[0] &= ~0x40;    /* 关闭时钟1Hz频率输出 */
            i2c_buf[1] &= ~0x20;    /* 清除时钟更新标志位 */
            i2c_buf[2] &= ~0x30;    /* 关闭时钟中断输出 */
        } else {
            break;
        }
    case DO_I2C_WRITE_1:
        bsp_sta = hi2c0.i2c_ops.write(&hi2c0, 0x1D, i2c_buf, 3);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR)
                goto _RET_ERROR;
            state = DO_I2C_WRITE_2;
            i2c_buf[0] = 0x05;
        } else {
            break;
        }
    case DO_I2C_WRITE_2:
        bsp_sta = hi2c0.i2c_ops.write(&hi2c0, 0x32, i2c_buf, 1);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR)
                goto _RET_ERROR;
            hi2c0.busy = 0;
            state = IS_I2C_BUSY;
            return BSP_OK;
        }
        break;
    default:
        break;
    }

    return bsp_sta;
_RET_ERROR:
    hi2c0.busy = 0;
    state = IS_I2C_BUSY;
    return BSP_ERROR;
}
