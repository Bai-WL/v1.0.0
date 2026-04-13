/**
 * @file bsp_gd32f303xx_mtxkey_poll.c
 * @author your name (you@domain.com)
 * @brief gd32f303 外部中断触发的矩阵键盘扫描
 * @version 0.3
 * @date 2025-3-11
 * 
 * @copyright Copyright (c) 2024-2025
 * 
 */
#include "gd32f30x_gpio.h"
#include "gd32f30x_rcu.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f303xx_mtxkey_poll.h"
#include "bsp.h"

#define gpio_set_level(_GPIO_PORT, _GPIO_PIN, _A)  do { \
            _A ? gpio_bit_set(_GPIO_PORT, _GPIO_PIN) : gpio_bit_reset(_GPIO_PORT, _GPIO_PIN); \
} while(0)

#define get_mtx_key_y1() gpio_input_bit_get(MTX_KEY_Y1_PORT, MTX_KEY_Y1_PIN)
#define get_mtx_key_y2() gpio_input_bit_get(MTX_KEY_Y2_PORT, MTX_KEY_Y2_PIN)
#define get_mtx_key_y3() gpio_input_bit_get(MTX_KEY_Y3_PORT, MTX_KEY_Y3_PIN)
#define get_mtx_key_y4() gpio_input_bit_get(MTX_KEY_Y4_PORT, MTX_KEY_Y4_PIN)

#define set_mtx_key_x1(_X) gpio_set_level(MTX_KEY_X1_PORT, MTX_KEY_X1_PIN, _X)
#define set_mtx_key_x2(_X) gpio_set_level(MTX_KEY_X2_PORT, MTX_KEY_X2_PIN, _X)
#define set_mtx_key_x3(_X) gpio_set_level(MTX_KEY_X3_PORT, MTX_KEY_X3_PIN, _X)
#define set_mtx_key_x4(_X) gpio_set_level(MTX_KEY_X4_PORT, MTX_KEY_X4_PIN, _X)

static uint8_t ucScanAllRow(void);
static uint8_t ucConfirmThatRow(uint8_t _ucRow);
static uint8_t ucScanAllColOfThatRow(uint8_t _ucRow);
static uint8_t ucConfirmThatColOfThatRow(uint8_t _ucRow, uint8_t _ucCol);
static uint8_t ucConfirmThatMtxKey(uint8_t _ucRow, uint8_t _ucCol);

/**
 * @brief 矩阵键盘扫描IO初始化
 * 
 */
void bsp_MtxKeyPollInit(void)
{
    rcu_periph_clock_enable(RCU_MTX_KEY_X1);
    rcu_periph_clock_enable(RCU_MTX_KEY_X2);
    rcu_periph_clock_enable(RCU_MTX_KEY_X3);
    rcu_periph_clock_enable(RCU_MTX_KEY_X4);
    rcu_periph_clock_enable(RCU_MTX_KEY_Y1);
    rcu_periph_clock_enable(RCU_MTX_KEY_Y2);
    rcu_periph_clock_enable(RCU_MTX_KEY_Y3);
    rcu_periph_clock_enable(RCU_MTX_KEY_Y4);
    rcu_periph_clock_enable(RCU_AF);

    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    gpio_init(MTX_KEY_X1_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, MTX_KEY_X1_PIN);  // key_x0
    gpio_init(MTX_KEY_X2_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, MTX_KEY_X2_PIN);  // key_x1
    gpio_init(MTX_KEY_X3_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, MTX_KEY_X3_PIN);  // key_x2
    gpio_init(MTX_KEY_X4_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, MTX_KEY_X4_PIN);  // key_x3

    gpio_bit_reset(MTX_KEY_X1_PORT, MTX_KEY_X1_PIN);  // key_x0
    gpio_bit_reset(MTX_KEY_X2_PORT, MTX_KEY_X2_PIN);  // key_x1
    gpio_bit_reset(MTX_KEY_X3_PORT, MTX_KEY_X3_PIN);  // key_x2
    gpio_bit_reset(MTX_KEY_X4_PORT, MTX_KEY_X4_PIN);  // key_x3

    gpio_init(MTX_KEY_Y1_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, MTX_KEY_Y1_PIN); // key_y0
    gpio_init(MTX_KEY_Y2_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, MTX_KEY_Y2_PIN);  // key_y1
    gpio_init(MTX_KEY_Y3_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, MTX_KEY_Y3_PIN);  // key_y2
    gpio_init(MTX_KEY_Y4_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, MTX_KEY_Y4_PIN);  // key_y3
}

/**
 * @brief 扫描矩阵键盘，返回矩阵键盘按下的按键值
 * 
 * @return uint8_t 0:无按键按下；非0: 矩阵键盘对应按下按键的键值
 */
uint8_t bsp_ucScanMtxKey(void)
{
    static enum {
        SCAN_KEY = 0,
        FILTER0,
        KEY_PRSED,
        FILTER1
    } MtxKeyState = SCAN_KEY; 
    static uint8_t nRow_cur = 0;
    static uint8_t nCol_cur = 0;
    static uint32_t lastTick = 0;

    switch (MtxKeyState) {
    case SCAN_KEY:
        nRow_cur = ucScanAllRow();
        if (nRow_cur) {
            lastTick = bsp_get_systick();
            MtxKeyState = FILTER0;
        }
        break;
    case FILTER0:
        if ((bsp_get_systick() - lastTick) > 10) {
            if (ucConfirmThatRow(nRow_cur)) {
                nCol_cur = ucScanAllColOfThatRow(nRow_cur);
                if (nCol_cur) {
                    MtxKeyState = KEY_PRSED;
                } else {    /* 双键情况也不进行扫描 */
                    MtxKeyState = SCAN_KEY;
                }
            } else {
                MtxKeyState = SCAN_KEY;
            }
        }
        break;
    case KEY_PRSED:
        dont_sleep = 1;
        if (!ucConfirmThatMtxKey(nRow_cur, nCol_cur)) {
            lastTick = bsp_get_systick();
            MtxKeyState = FILTER1;
        }
        break;
    case FILTER1:
        if ((bsp_get_systick() - lastTick) > 10) {
            if (!ucConfirmThatMtxKey(nRow_cur, nCol_cur)) {
                nRow_cur = 0;
                nCol_cur = 0;
                MtxKeyState = SCAN_KEY;
            } else {
                MtxKeyState = KEY_PRSED;
            }
        }
        break;
    default:
        break;
    }

    if (nCol_cur) {
        return ((nRow_cur-1)*4+nCol_cur);
    } else {
        return 0;
    }
}

/**
 * @brief 扫描是否有按键按下，并返回对应行号
 * 
 * @return uint8_t 0: 无按键按下；非0: 对应按下按键的行坐标
 */
static uint8_t ucScanAllRow(void)
{
    if (!get_mtx_key_y1()) {
        return 1;
    } else if (!get_mtx_key_y2()) {
        return 2;
    } else if (!get_mtx_key_y3()) {
        return 3;
    } else if (!get_mtx_key_y4()) {
        return 4;
    } else {
        return 0;
    }
}

/**
 * @brief 检查改行中是否还存在按下的按键
 * 
 * @param _ucRow 
 * @return uint8_t 是否按下，而非实际IO电平值; 1 = 按下， 0 = 未按下
 */
static uint8_t ucConfirmThatRow(uint8_t _ucRow)
{
    switch (_ucRow) {
    case 1:
        return !get_mtx_key_y1();
    case 2:
        return !get_mtx_key_y2();
    case 3:
        return !get_mtx_key_y3();
    case 4:
        return !get_mtx_key_y4();
    default:
        return 0;
    }
}

/**
 * @brief 扫描该行中按下按键的对应列坐标
 * 
 * @param _ucRow 
 * @return uint8_t 0: 存在多个按下的按键：非0: 该行中按下按键的对应列坐标
 */
static uint8_t ucScanAllColOfThatRow(uint8_t _ucRow) {
    uint8_t i, a;

    for (i = 1; i <= 4; i++) {
        a = ucConfirmThatColOfThatRow(_ucRow, i);
        if (a) 
            break;
    }
    set_mtx_key_x1(0);
    set_mtx_key_x2(0);
    set_mtx_key_x3(0);
    set_mtx_key_x4(0);
    if (!a) 
        i = 0;
    return i;
}

/**
 * @brief 确认该列坐标是否为该行中被按下按键的对应列坐标
 * 
 * @param _ucRow 
 * @param _ucCol 
 * @return uint8_t 0: 否; 1: 是
 */
static uint8_t ucConfirmThatColOfThatRow(uint8_t _ucRow, uint8_t _ucCol) 
{
    uint8_t a;
    switch (_ucCol) {
        case 1: {
            set_mtx_key_x1(1);  // key_x0
            set_mtx_key_x2(0);  // key_x1
            set_mtx_key_x3(0);  // key_x2
            set_mtx_key_x4(0);  // key_x3
        } break;
        case 2: {
            set_mtx_key_x1(0);  // key_x0
            set_mtx_key_x2(1);  // key_x1
            set_mtx_key_x3(0);  // key_x2
            set_mtx_key_x4(0);  // key_x3
        } break;
        case 3: {
            set_mtx_key_x1(0);  // key_x0
            set_mtx_key_x2(0);  // key_x1
            set_mtx_key_x3(1);  // key_x2
            set_mtx_key_x4(0);  // key_x3
        } break;
        case 4: {
            set_mtx_key_x1(0);  // key_x0
            set_mtx_key_x2(0);  // key_x1
            set_mtx_key_x3(0);  // key_x2
            set_mtx_key_x4(1);  // key_x3
        } break;
        default: break;
    }

    a = ucConfirmThatRow(_ucRow);

    return !a;  /* 此处的列扫描判定条件为：扫描结果为未按下，说明正是该列的按钮按下，故此处a取反 */
}

/**
 * @brief 
 * 
 * @param _ucRow 
 * @param _ucCol 
 * @return uint8_t 该按键是否按下
 */
static uint8_t ucConfirmThatMtxKey(uint8_t _ucRow, uint8_t _ucCol)
{
    uint8_t a;
    if (!ucConfirmThatRow(_ucRow)) 
        return 0;
    a = ucConfirmThatColOfThatRow(_ucRow, _ucCol);
    set_mtx_key_x1(0);  // key_x0
    set_mtx_key_x2(0);  // key_x1
    set_mtx_key_x3(0);  // key_x2
    set_mtx_key_x4(0);  // key_x3
    return a;
}
