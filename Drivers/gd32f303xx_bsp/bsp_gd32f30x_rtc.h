/**
 * @file bsp_gd32f30x_rtc.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-10
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __BSP_GD32F30X_RTC_H__
#define __BSP_GD32F30X_RTC_H__
#include "stdint.h"
#include "bsp_types.h"

struct realtime_t {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    volatile uint8_t valid;  /* Êý¾Ý»¥³âËø */
};

/**
 * @brief two-digit to bcd code
 * 
 * @param num 
 * @return uint8_t 
 */
static inline uint8_t num2bcd(uint8_t num) {
    return ((((num/10)%10)<<4) | (num%10));
}

static inline uint8_t bcd2num(uint8_t bcd) {
    return (bcd>>4)*10 + (bcd&0xF);
}

void bsp_RTC_Init(void);
BSP_StatusTypedef bsp_RTCSetRealTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
BSP_StatusTypedef bsp_RefreshRealTime(void);    /* run in bsp_RunPer1ms()*/
void bsp_CaptureSecondInterrupt(void); /* run in EXTI5_IRQn() */
struct realtime_t* bsp_RTCGetTime(void) ;
void bsp_RTCRunPer1ms(void);

#endif /* __BSP_GD32F30X_RTC_H__ */
