/**
 * @file drv_hid.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-29
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_misc.h"
#include "gd32f30x_timer.h"

#include "bsp_gd32f303xx_systick.h"
#include "drv_hid.h"
#include "drv_pm.h"

/**
 * @brief human interface device driver init
 * 
 */
void hid_init(void)
{
    static void bsp_FunKeyInit(void);
    static void bsp_MtxKeyPollInit(void);
    static void bsp_knob_init(void);

    bsp_FunKeyInit();
    bsp_MtxKeyPollInit();
    bsp_knob_init();
}

static uint8_t FunKeyVal = 0;
static uint8_t MtxKeyVal = 0;
static int32_t knob_cnt = 0;
static int32_t last_knob_cnt = 0;
/**
 * @brief human interface device scan 
 *  Note: It is recommended to run this function in a 1ms period event.
 * 
 */
void hid_scan(void)
{
    static uint8_t bsp_ucScanFunKey(void);
    static uint8_t bsp_ucScanMtxKey(void);
    static void bsp_KnobCntScan(void);

    last_knob_cnt = knob_cnt;

    /* function key scan */
    FunKeyVal = bsp_ucScanFunKey();
    /* matrix key scan */
    MtxKeyVal = bsp_ucScanMtxKey();
    /* rotary knob scan */
    bsp_KnobCntScan();

    /* dont sleep scan */
    if ((last_knob_cnt != knob_cnt) || FunKeyVal || MtxKeyVal)
        pm_dont_sleep();
}

uint8_t hid_get_funkey(void)
{
    return FunKeyVal;
}

uint8_t hid_get_mtxkey(void)
{
    return MtxKeyVal;
}

uint32_t hid_get_knobcnt(void)
{
    return -knob_cnt;
}

/***************************** Function Key *****************************/
static void bsp_FunKeyInit(void)
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

static uint8_t ucScanAllFunKey(void);
static uint8_t ucConfirmThatFunKey(uint8_t _ucKeyNum);
/**
 * @brief 功能按键扫描函数
 * 
 * @return uint8_t 0：未按下；非0：对应键值
 */
static uint8_t bsp_ucScanFunKey(void)
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

#define get_key_f1() gpio_input_bit_get(FUN_KEY_F1_PORT, FUN_KEY_F1_PIN)
#define get_key_f2() gpio_input_bit_get(FUN_KEY_F2_PORT, FUN_KEY_F2_PIN)
#define get_key_f3() gpio_input_bit_get(FUN_KEY_F3_PORT, FUN_KEY_F3_PIN)
#define get_key_f4() gpio_input_bit_get(FUN_KEY_F4_PORT, FUN_KEY_F4_PIN)
#define get_key_f5() gpio_input_bit_get(FUN_KEY_F5_PORT, FUN_KEY_F5_PIN)
#define get_key_f6() gpio_input_bit_get(FUN_KEY_F6_PORT, FUN_KEY_F6_PIN)

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

/***************************** Matrix Key *****************************/
/**
 * @brief 矩阵键盘扫描IO初始化
 * 
 */
static void bsp_MtxKeyPollInit(void)
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

static uint8_t ucScanAllRow(void);
static uint8_t ucConfirmThatRow(uint8_t _ucRow);
static uint8_t ucScanAllColOfThatRow(uint8_t _ucRow);
static uint8_t ucConfirmThatColOfThatRow(uint8_t _ucRow, uint8_t _ucCol);
static uint8_t ucConfirmThatMtxKey(uint8_t _ucRow, uint8_t _ucCol);
/**
 * @brief 扫描矩阵键盘，返回矩阵键盘按下的按键值
 * 
 * @return uint8_t 0:无按键按下；非0: 矩阵键盘对应按下按键的键值
 */
static uint8_t bsp_ucScanMtxKey(void)
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

/***************************** Rotary Knob *****************************/
/**
 * @brief 正交译码器初始化
 * 
 */
static void bsp_knob_init(void)
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_TIMER3);
    
    /* 配置TIMER3_CH0和TIMER3_CH1 */
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6|GPIO_PIN_7);  

    timer_deinit(TIMER3);

    timer_initpara.period = 0xFFFF;
    timer_initpara.prescaler = 0;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    
    timer_init(TIMER3, &timer_initpara);
    timer_update_source_config(TIMER3, TIMER_UPDATE_SRC_REGULAR);
    timer_quadrature_decoder_mode_config(TIMER3, TIMER_ENCODER_MODE0, TIMER_IC_POLARITY_RISING, TIMER_IC_POLARITY_RISING);
    timer_auto_reload_shadow_enable(TIMER3);
    
	timer_counter_value_config(TIMER3, 0);

    timer_enable(TIMER3);
}


static int32_t g_ulTimOvfCnt = 0;
/**
 * @brief 将该函数放置于定时器中，不使用溢出中断，手动判定定时器溢出，规避芯片上的中断bug。
 * 
 */
static void bsp_KnobCntScan(void)
{
    static int16_t last_cnt = 0x0000;
    int16_t cur_cnt = (int16_t)timer_counter_read(TIMER3);
    if ((last_cnt >= 0) != (cur_cnt >= 0)) {  /* 判定是否存在溢出 */
        if ((cur_cnt - last_cnt) > 0) {
			g_ulTimOvfCnt++;
		} else {
			g_ulTimOvfCnt--;
		}
    }	

	last_cnt = cur_cnt;
	knob_cnt = (uint16_t)cur_cnt + g_ulTimOvfCnt*65536;
}

/**
 * @brief 将编码旋钮累积计数值重置为0
 * 
 */
void bsp_rstKnobCnt(void)
{
    /* stop timer */
    timer_disable(TIMER3);
    /* get timer counts */
    timer_counter_value_config(TIMER3, 0);
    g_ulTimOvfCnt = 0;
    knob_cnt = 0;
    /* enable timer */
    timer_enable(TIMER3);
}
