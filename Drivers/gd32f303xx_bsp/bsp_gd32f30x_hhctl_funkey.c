/**
 * @file bsp_gd32f30x_hhctl_funkey.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2025-06-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x_gpio.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f30x_hhctl_funkey.h"
#include "bsp.h"

#define get_key_f1() gpio_input_bit_get(FUN_KEY_F1_PORT, FUN_KEY_F1_PIN)
#define get_key_f2() gpio_input_bit_get(FUN_KEY_F2_PORT, FUN_KEY_F2_PIN)
#define get_key_f3() gpio_input_bit_get(FUN_KEY_F3_PORT, FUN_KEY_F3_PIN)
#define get_key_f4() gpio_input_bit_get(FUN_KEY_F4_PORT, FUN_KEY_F4_PIN)
#define get_key_f5() gpio_input_bit_get(FUN_KEY_F5_PORT, FUN_KEY_F5_PIN)
#define get_key_f6() gpio_input_bit_get(FUN_KEY_F6_PORT, FUN_KEY_F6_PIN)

static uint8_t ucScanAllFunKey(void);
static uint8_t ucConfirmThatFunKey(uint8_t _ucKeyNum);

void bsp_FunKeyInit(void)
{
    rcu_periph_clock_enable(RCU_FUN_KEY_F1);
    rcu_periph_clock_enable(RCU_FUN_KEY_F2);
    rcu_periph_clock_enable(RCU_FUN_KEY_F3);   
    rcu_periph_clock_enable(RCU_FUN_KEY_F4);
    rcu_periph_clock_enable(RCU_FUN_KEY_F5);
    rcu_periph_clock_enable(RCU_FUN_KEY_F6);

    gpio_init(FUN_KEY_F1_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F1_PIN);  // key_f1
    gpio_init(FUN_KEY_F2_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F2_PIN);  // key_f2
    gpio_init(FUN_KEY_F3_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F3_PIN);  // key_f3
    gpio_init(FUN_KEY_F4_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F4_PIN);  // key_f4
    gpio_init(FUN_KEY_F5_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F5_PIN);  // key_f5
    gpio_init(FUN_KEY_F6_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, FUN_KEY_F6_PIN);  // key_f6
}

/**
 * @brief 功能按键扫描函数
 * 
 * @return uint8_t 0：未按下；非0：对应键值
 */
uint8_t bsp_ucScanFunKey(void)
{
    static enum {
        SCAN_KEY = 0,
        FILTER0,
        KEY_PRSED,
        FILTER1
    } FunKeyState = SCAN_KEY; 
    static uint32_t lastTick = 0;
    static uint8_t key_cur = 0;

    switch (FunKeyState)
    {
    case SCAN_KEY:
        key_cur = ucScanAllFunKey();
        if (key_cur) {
            lastTick = bsp_get_systick();
            FunKeyState = FILTER0;
        }
        break;
    case FILTER0:
        if ((bsp_get_systick() - lastTick) > 10) {
            if (ucConfirmThatFunKey(key_cur)) {
                FunKeyState = KEY_PRSED;
            } else {
                FunKeyState = SCAN_KEY;
            }
        }
        break;
    case KEY_PRSED:
        dont_sleep = 1;
        if (!ucConfirmThatFunKey(key_cur)) {
            lastTick = bsp_get_systick();
            FunKeyState = FILTER1;
        }
        break;
    case FILTER1:
        if ((bsp_get_systick() - lastTick) > 10) {
            if (!ucConfirmThatFunKey(key_cur)) {
                key_cur = 0;
                FunKeyState = SCAN_KEY;
            } else {
                FunKeyState = KEY_PRSED;
            }
        }
        break;
    default:
        break;
    }

    return key_cur;
}

/**
 * @brief 扫描所有按键，返回键值
 * 
 * @return uint8_t 键值
 */
static uint8_t ucScanAllFunKey(void)
{
    if (!get_key_f1()) {
        return 1;
    } else if (!get_key_f2()) {
        return 2;
    } else if (!get_key_f3()) {
        return 3;
    } else if (!get_key_f4()) {
        return 4;
    } else if (!get_key_f5()) {
        return 5;
    } else if (!get_key_f6()) {
        return 6;
    } else {
		return 0;
	}
}

/**
 * @brief 确认按键是否按下
 * 
 * @param _ucKeyNum 
 * @return uint8_t 0: 否；1: 是
 */
static uint8_t ucConfirmThatFunKey(uint8_t _ucKeyNum)
{
    switch (_ucKeyNum) {
    case 1:
        return !get_key_f1();
    case 2:
        return !get_key_f2();
    case 3:
        return !get_key_f3();
    case 4:
        return !get_key_f4();
    case 5:
        return !get_key_f5();
    case 6:
        return !get_key_f6();
	default:
		return 0;
    }
}
