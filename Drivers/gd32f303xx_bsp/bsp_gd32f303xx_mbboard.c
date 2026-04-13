/**
 * @file bsp_gd32f303xx_mbuser.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stddef.h>
#include "gd32f30x_dma.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_usart.h"
#include "gd32f30x_timer.h"
#include "gd32f30x_misc.h"
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_gd32f303xx_mbboard.h"

#define USART1_DATA_ADDRESS    ((uint32_t) 0x40004404)

void bsp_MBTIMInit(void) 
{
    uint32_t apb1_clk = rcu_clock_freq_get(CK_APB1);

    timer_parameter_struct timer_initpara;
    /* enable modbus timer */
    rcu_periph_clock_enable(RCU_TIMER4);
    timer_deinit(TIMER4);

	/* GD32的时钟倍频器与APBx分频器相耦合，当APB1分频配置为2分频时，倍频器对应2倍频，使定时器输入时钟为AHB */
    timer_initpara.period = apb1_clk/1000 - 1;
    timer_initpara.prescaler = 20 - 1;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    
    timer_init(TIMER4, &timer_initpara);
    timer_update_source_config(TIMER4, TIMER_UPDATE_SRC_REGULAR);
    timer_auto_reload_shadow_enable(TIMER4);
    
	timer_counter_value_config(TIMER4, 0);
	timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
	timer_interrupt_enable(TIMER4, TIMER_INT_UP);
	nvic_irq_enable(TIMER4_IRQn, 2, 0);     /* 定时的要求不严格，因此中断优先级可以靠后 */

    // timer_enable(TIMER4);
}

void bsp_MBHardInit(uint32_t _ulBaud)
{
    dma_parameter_struct dma_init_struct;

    /* enable usart1 */
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_USART1);

    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    usart_deinit(USART1);
    usart_baudrate_set(USART1, _ulBaud);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);

    nvic_irq_enable(USART1_IRQn, 1, 0);
    usart_flag_clear(USART1, USART_FLAG_RBNE);
    usart_interrupt_enable(USART1, USART_INT_RBNE);
	usart_flag_clear(USART1, USART_FLAG_TC);
    usart_enable(USART1);

	/* 485 R/D GOIO enable */
	gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
	gpio_bit_reset(GPIOB, GPIO_PIN_12);

    /* enable DMA0 */
    rcu_periph_clock_enable(RCU_DMA0);
    dma_struct_para_init(&dma_init_struct);
    dma_deinit(DMA0, DMA_CH6);

    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = NULL;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = 0;
    dma_init_struct.periph_addr = USART1_DATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH6, &dma_init_struct); /* DMA0 CH6 USART1_TX */

    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_memory_to_memory_disable(DMA0, DMA_CH6);
}

BSP_StatusTypedef bsp_MBDMASend_Polling(const uint8_t *_pcBuf, uint32_t _usLen)
{
    MB_TxState();

	usart_flag_clear(USART1, USART_FLAG_TC);
	
    dma_channel_disable(DMA0, DMA_CH6);

	dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);
	dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_ERR);

    dma_memory_address_config(DMA0, DMA_CH6, (uint32_t)_pcBuf);
    dma_transfer_number_config(DMA0, DMA_CH6, _usLen);

    dma_channel_enable(DMA0, DMA_CH6);

    usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);
	
	while (RESET == dma_flag_get(DMA0, DMA_CH6, DMA_FLAG_FTF));
	dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);
    while (RESET == usart_flag_get(USART1, USART_FLAG_TC));
	usart_flag_clear(USART1, USART_FLAG_TC);

    MB_RxState();

    return BSP_OK;
}

//BSP_StatusTypedef bsp_MBDMASend_Polling(const uint8_t *_pcBuf, uint16_t _usLen)
//{
//    static uint32_t dma_send_sta = 0;

//    switch (dma_send_sta) {
//    case 0:
//        MB_TxState();
//        usart_flag_clear(USART1, USART_FLAG_TC);
//        
//        dma_channel_disable(DMA0, DMA_CH6);
//        dma_memory_address_config(DMA0, DMA_CH6, (uint32_t)_pcBuf);
//        dma_transfer_number_config(DMA0, DMA_CH6, _usLen);
//        dma_channel_enable(DMA0, DMA_CH6);
//        usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);
//        
//        dma_send_sta = 1;
//    case 1:
//        if (SET == dma_flag_get(DMA0, DMA_CH6, DMA_FLAG_FTF)) {
//            dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);
//            dma_send_sta = 1;
//        } else {
//            break;
//        }
//    case 2:
//        if (SET == usart_flag_get(USART1, USART_FLAG_TC)) {
//            usart_flag_clear(USART1, USART_FLAG_TC);
//            dma_send_sta = 0;
//            MB_RxState();
//            return BSP_OK;
//        } else {
//            break;
//        }
//    default:
//        break;
//    }
//    return BSP_BUSY;
//}

void bsp_485_rst_trans_chan(void)
{
    __NOP();
}
