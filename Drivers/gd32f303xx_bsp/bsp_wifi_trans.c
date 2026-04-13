/**
 * @file bsp_wifi_trans.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.5
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "gd32f30x_dma.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_usart.h"
#include "gd32f30x_timer.h"
#include "gd32f30x_misc.h"
#include "bsp_wifi_trans.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_types.h"
#include "bsp_malloc.h"
#include "bsp_fifo.h"

#define USART2_DATA_ADDRESS    ((uint32_t) 0x40004804)
#define wifi_send_data_start usart2_dma_send_start
#define WIFI_PWR(x)   do { x ? \
                            gpio_bit_set(GPIOA, GPIO_PIN_1)   :\
                            gpio_bit_reset(GPIOA, GPIO_PIN_1) ;\
                        } while(0)

static void usart2_dma_send_start(const uint8_t *txbuf, uint16_t len);
static BSP_StatusTypedef wifi_trans_data(struct wifi_driver *wifi_drv, uint32_t mutex, const char *_pcmd, uint32_t _cmdlen, const char *_pcmp, uint32_t _cmplen, char* rxbuf, uint32_t timeout);

/**
 * @brief usart2 peripherial init
 * 
 * @param _ulBaud 
 */
static void usart2_init(uint32_t _ulBaud)
{
	rcu_periph_clock_enable(RCU_GPIOB);
	rcu_periph_clock_enable(RCU_USART2);
	rcu_periph_clock_enable(RCU_AF);

	gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_11);
	gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

	usart_deinit(USART2);
	usart_baudrate_set(USART2, _ulBaud);
	usart_receive_config(USART2, USART_RECEIVE_ENABLE);
	usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
	
	nvic_irq_enable(USART2_IRQn, 0x01, 0);
    usart_flag_clear(USART2, USART_FLAG_RBNE);
    usart_interrupt_enable(USART2, USART_INT_RBNE);
	
    usart_enable(USART2);
}

/**
 * @brief usart2 dma init
 * 
 */
static void usart2_dma_init(void)
{
	dma_parameter_struct dma_init_struct;
	/* enable DMA0 */
	rcu_periph_clock_enable(RCU_DMA0);
	dma_struct_para_init(&dma_init_struct);
	dma_deinit(DMA0, DMA_CH1);

	dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;	
	dma_init_struct.memory_addr = NULL;
	dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
	dma_init_struct.number = 0;
	dma_init_struct.periph_addr = USART2_DATA_ADDRESS;
	dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	dma_init_struct.priority = DMA_PRIORITY_HIGH;
	dma_init(DMA0, DMA_CH1, &dma_init_struct);
	
	dma_circulation_disable(DMA0, DMA_CH1);
	dma_memory_to_memory_disable(DMA0, DMA_CH1);
}

void bsp_wifi_driver_init(struct wifi_driver *wifi_drv, uint32_t _ulBaud, struct wifi_driver_callbacks *cbs)
{
    /* 硬件层 */
    wifi_drv->trans_type = WIFI_NORMAL_TRANS,
    wifi_drv->trans_state = WIFI_GET_DONE,
    wifi_drv->cur_mutex = 0;
    wifi_drv->last_tick = 0;
    wifi_drv->cmpstr = NULL,
    wifi_drv->cmpstr_window_bias = 0,
    wifi_drv->cur_mutex = 0,
    wifi_drv->rxbuf = NULL,
    wifi_drv->rxcnt = 0;
    wifi_drv->wifi_drv_cbs = cbs;

    usart2_init(_ulBaud);
    usart2_dma_init();
    /* WIFI hardware reset */
    rcu_periph_clock_enable(RCU_GPIOB); 
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
	gpio_bit_reset(GPIOB, GPIO_PIN_2);
	bsp_delay(100);
	gpio_bit_set(GPIOB, GPIO_PIN_2);
	bsp_delay(300);
}

//static void bsp_wifi_trans_reset(struct wifi_driver *wifi_drv)  /* DEBUG: 20250728 暂时不作使用 */
//{
//    wifi_drv->cur_mutex = 0;
//    wifi_trans_data(wifi_drv, 0, NULL, 0, NULL, 0, NULL, 0);
//}

void bsp_wifi_atcmd_response_parser(struct wifi_driver *wifi_drv, const uint8_t _ucChar)
{
    static uint8_t last_char;

    switch (wifi_drv->trans_state) {
    case WIFI_GET_CMPSTR:
        if (wifi_drv->cmpstr == NULL) 
            break;
        if (_ucChar == wifi_drv->cmpstr[wifi_drv->cmpstr_window_bias]) {
            wifi_drv->cmpstr_window_bias++;
        } else {
            wifi_drv->cmpstr_window_bias = 0;
            if (_ucChar == wifi_drv->cmpstr[wifi_drv->cmpstr_window_bias]) {
                wifi_drv->cmpstr_window_bias++;
            }
        }
        /* CMPSTR 完全匹配 */
        if (wifi_drv->cmpstr[wifi_drv->cmpstr_window_bias] == '\0') {
            wifi_drv->trans_state = WIFI_GET_DATA;
            wifi_drv->rxcnt = 0;
            if (wifi_drv->rxbuf == NULL) {
                wifi_drv->trans_state = WIFI_GET_DONE;
            }
        }
        break;
    case WIFI_GET_DATA:
        if (wifi_drv->rxcnt < 512 - 1) { /* -1是为了把最后一个位置留给结束符, 且避免越界 */ /* TODO: 在调用传输函数时，需要对缓冲区大小进行声明 */ /* TODO：接收缓冲区代码安全性优化 */
            wifi_drv->rxbuf[wifi_drv->rxcnt++] = _ucChar;
        } 
        if (_ucChar == 'K' && last_char == 'O') {
            wifi_drv->rxbuf[wifi_drv->rxcnt] = '\0';	/* 帧尾填结束符 */
            wifi_drv->rxbuf = NULL; /* 解除接收缓冲区绑定 */
            wifi_drv->trans_state = WIFI_GET_DONE;
        }
        break;
    case WIFI_GET_DONE:
    default:
        break;
    }
    last_char = _ucChar;
}

char debug_char[512] = {0};
char wifi_debug_char[1024] = {0};
uint32_t star_send_tick = 0;
uint32_t last_send_tick = 0;
uint32_t atcmd_tick_cnt = 0;
uint32_t cmd_idx = 0;
uint32_t dbg_idx = 0;
uint32_t all_idx = 0;
uint32_t orerr_times = 0;
/**
 * @brief 
 * 
 */
void bsp_wifi_byte_stream_isr(struct wifi_driver *wifi_drv)
{
    uint8_t tmpChar;
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE_ORERR)) {
        tmpChar = (uint8_t)usart_data_receive(USART2); /* FIXME: 此处语句仅粗略解决接收溢出卡死问题，需要进行更合理的调整 */
        orerr_times++;
        bsp_wifi_atcmd_response_parser(wifi_drv, tmpChar);
		wifi_drv->wifi_drv_cbs->stream_handle_cb(tmpChar);
    } else if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE)) {
        tmpChar = (uint8_t)usart_data_receive(USART2);
        if (dbg_idx == 512) dbg_idx = 0;
        debug_char[dbg_idx++] = tmpChar;
		if (cmd_idx < 1024) 
			wifi_debug_char[cmd_idx++] = tmpChar;
		all_idx++;
		atcmd_tick_cnt = bsp_get_systick() - star_send_tick;

        bsp_wifi_atcmd_response_parser(wifi_drv, tmpChar);
		wifi_drv->wifi_drv_cbs->stream_handle_cb(tmpChar);
	}
}

/**
 * @brief send data by wifi without response
 * 
 * @param wifi_drv 
 * @param mutex 
 * @param buffer 
 * @param len 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_data_send(struct wifi_driver *wifi_drv, uint32_t mutex, const uint8_t *buffer, uint32_t len) 
{
    if (wifi_drv->trans_type != WIFI_NORMAL_TRANS) 
        return BSP_ERROR;   

    return wifi_trans_data(wifi_drv, mutex, (const char *)buffer, len, NULL, 0, NULL, 10);
}

/**
 * @brief wifi配置函数
 * 
 * @param cmd 
 * @param ans 
 * @param timeout 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_cmd_get_ans(struct wifi_driver *wifi_drv, uint32_t mutex, const char* cmd, const char* ans, uint32_t timeout)
{
    if (wifi_drv->trans_type != WIFI_NORMAL_TRANS) 
        return BSP_ERROR;

    return wifi_trans_data(wifi_drv, mutex, cmd, strlen(cmd), ans, strlen(ans), NULL, timeout);
}

/**
 * @brief wifi 获取配置信息函数
 * 
 * @param cmd 
 * @param param_head 
 * @param buffer 如果需要对长度无法确定，且可能超过512字节数的命令进行参数接收，提供的接收数组要大于512的大小，比如513；
 * @param timeout 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_cmd_get_param(struct wifi_driver *wifi_drv, uint32_t mutex, const char* cmd, const char* param_head, char *buffer, uint32_t timeout)
{
    if (wifi_drv->trans_type != WIFI_NORMAL_TRANS)  /* 判断当前传输是否处于透传状态 */ 
        return BSP_ERROR;

    return wifi_trans_data(wifi_drv, mutex, cmd, strlen(cmd), param_head, strlen(param_head), buffer, timeout);
}

/**
 * @brief 互斥锁用于在其他应用执行过程中实现AT命令收发
 * 
 * @return wifi_mutex_desc 
 */
wifi_mutex_desc bsp_wifi_mutex_trylock(struct wifi_driver *wifi_drv, wifi_mutex_desc mutex)
{
    if (wifi_drv->cur_mutex == 0) {
        wifi_drv->cur_mutex = bsp_get_systick();
		return wifi_drv->cur_mutex;
    } else if (mutex == wifi_drv->cur_mutex) {
        return mutex;
    }

    return 0;
}

/* TODO: at指令发送需要有间隔 */
void bsp_wifi_mutex_unlock(struct wifi_driver *wifi_drv, wifi_mutex_desc mutex) 
{
    if (mutex == wifi_drv->cur_mutex) {
        wifi_drv->cur_mutex = 0;
    }
}

uint32_t bsp_wifi_mutex_is_lock(struct wifi_driver *wifi_drv) {
    return wifi_drv->cur_mutex;
}

/**
 * @brief 
 * 
 * @param mutex 用于判定调用对象是否相同，避免发送完成状态被错误对象接收，影响其他语句的执行
 * @param _pdata 
 * @param _ulen 
 * @param read_lock 
 * @return BSP_StatusTypedef 
 */
static BSP_StatusTypedef wifi_trans_data(struct wifi_driver *wifi_drv, uint32_t mutex, const char *_pcmd, uint32_t _cmdlen, const char *_pcmp, uint32_t _cmplen, char* rxbuf, uint32_t timeout)
{
    static enum {   /* 用于锁存通道 */
        START_SEND = 0,
        WAIT_SEND_DONE_1,
        WAIT_SEND_DONE_2,
        GET_ANS,
        GET_DATA,
    } send_state = START_SEND;

    static char *pcmd = NULL;

    if (wifi_drv->cur_mutex == 0) {
        goto _RET_ERROR;    /* 重置现场 */
    }

    if (mutex == 0 || wifi_drv->cur_mutex != mutex) 
        return BSP_ERROR;   /* 保留现场, 不进行重置 */
	
    switch (send_state) {
    case START_SEND:
		// usart_interrupt_disable(USART2, USART_INT_RBNE);  /* 获取互斥锁后，再对通信参数进行配置 */
        if (_pcmp == NULL) {
            wifi_drv->trans_state = WIFI_GET_DONE;
        } else {
            wifi_drv->rxbuf = rxbuf;
            wifi_drv->cmpstr = bsp_malloc(_cmplen+1);
			if (wifi_drv->cmpstr == NULL) /* 防止跑飞，且便于debug */
				goto _RET_ERROR;
            memcpy(wifi_drv->cmpstr, _pcmp, _cmplen);
            wifi_drv->cmpstr[_cmplen] = '\0';
            wifi_drv->cmpstr_window_bias = 0;
            wifi_drv->trans_state = WIFI_GET_CMPSTR;
        }
		pcmd = bsp_malloc(_cmdlen);
		if (pcmd == NULL) /* 防止跑飞，且便于debug */
			goto _RET_ERROR;
        memcpy(pcmd, _pcmd, _cmdlen);
		
		memset(wifi_debug_char, 0x00, 1024);
		cmd_idx = 0;
		star_send_tick = bsp_get_systick();
		
        wifi_send_data_start((uint8_t *)pcmd, _cmdlen);
        // usart_flag_clear(USART2, USART_FLAG_RBNE);
        // usart_interrupt_enable(USART2, USART_INT_RBNE);
        send_state = WAIT_SEND_DONE_1;
        break;
    case WAIT_SEND_DONE_1:
        if (SET == dma_flag_get(DMA0, DMA_CH1, DMA_FLAG_FTF)) {
            dma_flag_clear(DMA0, DMA_CH1, DMA_FLAG_FTF);
            send_state = WAIT_SEND_DONE_2;
        } else {
            break;
        }
    case WAIT_SEND_DONE_2:
        if (SET == usart_flag_get(USART2, USART_FLAG_TC)) {
            usart_flag_clear(USART2, USART_FLAG_TC);
            bsp_free(pcmd);
            wifi_drv->last_tick = bsp_get_systick();
            send_state = GET_ANS;
            // if (!read_lock) {   /* 不进行读锁定，若进行读锁定，则不对cur_mutex进行释放，避免另一进程进行数据发送，干扰数据的接收 */
            //     wifi_drv->cur_mutex = 0;
            // }
        } else {
            break;
        }
    case GET_ANS:
		if ((bsp_get_systick() - wifi_drv->last_tick) > timeout) {
			goto _RET_ERROR;
		}
        if (wifi_drv->trans_state >= WIFI_GET_DATA) {
            bsp_free(wifi_drv->cmpstr);
            send_state = GET_DATA;
        } else {
            break; /* 必须 break，避免在两次判断中间出现值改变 */
        }
    case GET_DATA:
        if ((bsp_get_systick() - wifi_drv->last_tick) > timeout) {
			goto _RET_ERROR;
		} 
        if (wifi_drv->trans_state == WIFI_GET_DONE) {
            send_state = START_SEND;
            // usart_interrupt_disable(USART2, USART_INT_RBNE);
            return BSP_OK;
        }
        break;
    default :
        goto _RET_ERROR;
    } 

    return BSP_BUSY;
_RET_ERROR:
	send_state = START_SEND;
    bsp_free(pcmd);
    bsp_free(wifi_drv->cmpstr);
    // usart_interrupt_disable(USART2, USART_INT_RBNE);
    return BSP_TIMEOUT;
}

/**
 * @brief 
 * 
 * @param txbuf 
 * @param len 
 */
static void usart2_dma_send_start(const uint8_t *txbuf, uint16_t len)
{
	usart_flag_clear(USART2, USART_FLAG_TC);

	dma_channel_disable(DMA0, DMA_CH1);
    
	dma_flag_clear(DMA0, DMA_CH1, DMA_FLAG_FTF);
	dma_flag_clear(DMA0, DMA_CH1, DMA_FLAG_ERR);
	
	dma_memory_address_config(DMA0, DMA_CH1, (uint32_t)txbuf);
	dma_transfer_number_config(DMA0, DMA_CH1, len);
	
	dma_channel_enable(DMA0, DMA_CH1);
	
    usart_dma_transmit_config(USART2, USART_TRANSMIT_DMA_ENABLE);
}

/**
 * @brief start unvarnished transmission mode
 * 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_start_unvarnished_txfer(struct wifi_driver *wifi_drv, uint32_t timeout) 
{
    const char cmd1[] = "AT+CIPMODE=1\r\n";
    const char cmd2[] = "AT+CIPSEND=1\r\n";
    static wifi_mutex_desc mutex = 0;   /* 用于锁存通道 */
    static enum {
        GET_MUTEX = 0,
        CMD1_TRANS,
        CMD2_TRANS
    } state = GET_MUTEX;
    BSP_StatusTypedef bsp_sta;

    if (wifi_drv->trans_type != WIFI_NORMAL_TRANS) 
        return BSP_ERROR;

    switch (state) {
    case GET_MUTEX:
		mutex = bsp_wifi_mutex_trylock(wifi_drv, mutex);
        if (mutex) { 
            /* change state */
            state = CMD1_TRANS;
        } else {
            break;
        }
    case CMD1_TRANS:
        if ((bsp_sta = wifi_trans_data(wifi_drv, mutex, cmd1, strlen(cmd1), "OK", 3, NULL, timeout)) != BSP_BUSY) {
            state = CMD2_TRANS;
            if (bsp_sta == BSP_ERROR)
                return bsp_sta;
        } else {
            break;
        }
    case CMD2_TRANS:
        if ((bsp_sta = wifi_trans_data(wifi_drv, mutex, cmd2, strlen(cmd2), "\r\n>", 4, NULL, timeout)) != BSP_BUSY) {
            bsp_wifi_mutex_unlock(wifi_drv, mutex);
            state = GET_MUTEX;
            return bsp_sta;
        } else {
            break;
        }
    default:
		break;
    }  

    return BSP_BUSY;
}

BSP_StatusTypedef bsp_wifi_stop_unvarnished_txfer(struct wifi_driver *wifi_drv) 
{
    /* 透传停止包需要单独发送，前沿至少20ms(才能被判定为单独包), 退出透传后，至少间隔1s再发送下一条AT指令 */
    static enum {
        GET_MUTEX = 0,
        WAIT_25MS,
        CMD_TRANS,
        WAIT_1S
    } state;
    static uint32_t mutex = 0;
    static uint32_t last_tick = 0;

    if (wifi_drv->trans_type != WIFI_UNVARNISHED_TRANS) 
        return BSP_ERROR;

    switch (state)
    {
    case GET_MUTEX:
		mutex = bsp_wifi_mutex_trylock(wifi_drv, mutex);
        if (mutex) {
            last_tick = bsp_get_systick();
            state = WAIT_25MS;
        } else {
            break;
        }
    case WAIT_25MS:
        if ((bsp_get_systick() - last_tick) >= 25) 
            state = CMD_TRANS;
        else 
            break;
    case CMD_TRANS:
        if (wifi_trans_data(wifi_drv, mutex, "+++", 3, NULL, 0, NULL, 1000)) {
            last_tick = bsp_get_systick();
            state = WAIT_1S;
        }
        break;
    case WAIT_1S:
        if ((bsp_get_systick() - last_tick) > 1000) {
            bsp_wifi_mutex_unlock(wifi_drv, mutex);
            state = GET_MUTEX;
            return BSP_OK;
        }
        break;
    default:
		break;
    }

    return BSP_BUSY;
}


/**
 * @brief 对于需要等待回复的发送线程，需要提前占用好wifi接收管道，获取接收标识符，以免错过待接收的数据
 * 
 */


/**
 * @brief 向读取管道申请一个读取标识符，避免所需的回复被错误对象接收
 *        wifi通信管道同一时间只能被一个对象占用
 *        管道内的接收数据只能由持有互斥锁的对象才可读取
 * @mutex: 互斥锁本质上是时间戳，当互斥锁为0时，说明通道中不存在互斥
 */

/*
	mutex_i = bsp_wifi_mutex_trylock(mutex_i);
	while (bsp_wifi_cmd_get_ans(mutex_i, "AT+CIPSTART=\"TCP\",\"192.168.16.88\",8080\r\n", "OK", 3000) != BSP_OK);
	while (bsp_wifi_cmd_get_ans(mutex_i, "AT+CIPSEND=8\r\n", "\r\n>", 3000) != BSP_OK);
	while (bsp_wifi_cmd_get_ans(mutex_i, "hello world", "", 10) != BSP_OK);
	bsp_wifi_mutex_unlock(mutex_i);

    mutex_i = bsp_wifi_mutex_trylock(mutex_i);
    if (bsp_wifi_cmd_get_param(mutex_i, "AT+UART_CUR?\r\n", "+UART_CUR:", g_RxBuffer0, 1000) == BSP_OK) {
        i++;
        bsp_wifi_mutex_unlock(mutex_i);
    }
    
    mutex_j = bsp_wifi_mutex_trylock(mutex_j);
    if (bsp_wifi_cmd_get_param(mutex_j, "AT+CWSAP_CUR?\r\n", "+CWSAP_CUR:", g_RxBuffer0, 1000) == BSP_OK) {
        j++;
        bsp_wifi_mutex_unlock(mutex_j);
    }
    
    mutex_k = bsp_wifi_mutex_trylock(mutex_k);
    if (bsp_wifi_cmd_get_param(mutex_k, "AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:", g_RxBuffer0, 1000) == BSP_OK) {
        k++;
        bsp_wifi_mutex_unlock(mutex_k);
	}
 */
