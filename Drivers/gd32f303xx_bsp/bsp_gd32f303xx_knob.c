/**
 * @file bsp_gd32f303xx_knob.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2024-2025
 * 
 */

#include <stdint.h>
#include "gd32f30x.h"
#include "gd32f30x_timer.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_misc.h"

static int32_t g_ulTimOvfCnt = 0;
static int32_t knob_cnt = 0;

/**
 * @brief 将该函数放置于定时器中，不使用溢出中断，手动判定定时器溢出，规避芯片上的中断bug。
 * 
 */
extern uint8_t dont_sleep;
void bsp_KnobCntRunPer1ms(void)
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

    if (last_cnt != cur_cnt)
        dont_sleep = 1;

	last_cnt = cur_cnt;
	knob_cnt = (uint16_t)cur_cnt + g_ulTimOvfCnt*65536;
}

/**
 * @brief 存在芯片硬件层次的bug，当发生定时器溢出中断时，若I2C正处在通信状态（HHCTL使用I2C0的PB8，PB9 GPIO口，刚好为TIMER3的CH2，CH3），
 *        会导致进入复数次更新中断，截至20250314，仍未找到解决方案。怀疑是硬件的复用功能上存在bug,导致了中断标志的误触发。
 * 
 */
void bsp_KnobTimIRQCallback(void)
{
	uint8_t dir = TIMER_CTL0(TIMER3) & TIMER_CTL0_DIR;
    if (RESET != timer_interrupt_flag_get(TIMER3, TIMER_INT_FLAG_UP)) {
		timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);	/* 在清除了标志位之后，仍然会在中断中再次置位 */	
        if (dir) {
            g_ulTimOvfCnt--;
        } else {
            g_ulTimOvfCnt++;
        }
    }
}

/**
 * @brief 获取当前编码旋钮累积计数值
 * 
 * @return int32_t 计数值
 */
int32_t bsp_getKnobCnt(void)
{
	return -knob_cnt;
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
    g_ulTimOvfCnt = 0;
    timer_counter_value_config(TIMER3, 0);
    /* enable timer */
    timer_enable(TIMER3);
}

/**
 * @brief 正交译码器初始化
 * 
 */
void bsp_knob_init(void)
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
	// timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP|TIMER_INT_FLAG_CH0|TIMER_INT_FLAG_CH1|TIMER_INT_FLAG_CH2|TIMER_INT_FLAG_CH3);
	// timer_interrupt_enable(TIMER3, TIMER_INT_UP);
	// nvic_irq_enable(TIMER3_IRQn, 0, 0);

    timer_enable(TIMER3);
}
