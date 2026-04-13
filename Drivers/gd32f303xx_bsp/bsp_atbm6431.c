/**
 * @file bsp_atbm6431.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.4
 * @date 2025-07-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"

#include "bsp_atbm6431.h"
#include "drv_pm.h"
#include "bsp_gd32f303xx_systick.h"

static void tcp_msg_parser_callback(uint8_t byte) 
{
    bsp_wifi_tcp_message_parser(&g_wifi_dev, byte);
}

static struct wifi_driver_callbacks wifi_drv_cbs = {
    .stream_handle_cb = tcp_msg_parser_callback
};

void bsp_atbm6431_init(uint32_t ulBaud)
{
    bsp_wifi_driver_init(&g_wifi_drv, ulBaud, &wifi_drv_cbs);
    bsp_wifi_dev_init(&g_wifi_dev, &g_wifi_drv);
}

/**
 * @brief 使用硬件复位引脚对atbm6431芯片进行复位，并重新配置热点扫描返回格式及多连接
 * 
 */
void bsp_atbm6431_reset(void)
{
    uint32_t state = 0;
    uint32_t mutex = 0;
    uint32_t timeout_times = 0;
    BSP_StatusTypedef bsp_sta = BSP_OK;

_RETRY_RST:
	state = 0;
	timeout_times = 0;
    rcu_periph_clock_enable(RCU_GPIOB); 
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
    gpio_bit_reset(GPIOB, GPIO_PIN_2);
    bsp_delay(100);
    gpio_bit_set(GPIOB, GPIO_PIN_2);
    bsp_delay(300);
    while (state < 2) {
        switch (state) {
        case 0:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+CWLAPOPT,1,6\r\n", "OK", 500);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 3) {
                    goto _RETRY_RST;
                }
            }
            break;
        case 1:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+CIPMUX=1\r\n", "OK", 1000);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 3) {
                    goto _RETRY_RST;
                }
            }
            break;
        default:
            state++;
            break;
        }
    }
}

static uint8_t dr_flag = 0; /* device reset flag */
void bsp_atbm6431_enter_sleepmode(void)
{
	uint32_t state = 2;
    uint32_t mutex = 0;
    uint32_t timeout_times = 0;
    BSP_StatusTypedef bsp_sta = BSP_OK;

    // bsp_wifi_trans_reset(&g_wifi_drv);
	if (g_wifi_dev.connect_state == HP_DISCONNECTED)
        goto _WIFI_HOLD_RST;

    while (state < 3) {
        switch (state) {
        case 0 :
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+WIFI_LISTEN_ITVL 0\r\n", "OK", 200);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }

                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) 
                    goto _WIFI_HOLD_RST;
            }
            break;
        case 1:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+GPIO_SET_DIR PIN 13 DIR 1\r\n", "OK", 200);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }

                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) 
                    goto _WIFI_HOLD_RST;
            }
            break;
        case 2:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+LIGHT_SLEEP 1\r\n", "OK", 300);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }

                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) 
                    goto _WIFI_HOLD_RST;
            }
            break;
        default:
            state++;
            break;
        }
    }

    return;
_WIFI_HOLD_RST:
    rcu_periph_clock_enable(RCU_GPIOB); 
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
	gpio_bit_reset(GPIOB, GPIO_PIN_2);
    dr_flag = 1;
}

void bsp_atbm6431_exit_sleepmode(void)
{
    uint32_t state = 1;
    uint32_t mutex = 0;
    uint32_t timeout_times = 0;
    BSP_StatusTypedef bsp_sta = BSP_OK;

    /* release wifi reset pin */
    if (dr_flag) 
        bsp_atbm6431_reset();

    while (state < 5) {
        switch (state) {
        case 0:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+LIGHT_SLEEP 0\r\n", "OK", 300);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) {
                    goto _WIFI_RST;
                }
            }
            break;
        case 1:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+MODEM_SLEEP 1\r\n", "OK", 300);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) {
                    goto _WIFI_RST;
                }
            }
            break;
        case 2:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+GPIO_SET_DIR PIN 13 DIR 0\r\n", "OK", 200);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 10) {
                    goto _WIFI_RST;
                }
            }
            break;
        case 3:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+CWLAPOPT,1,6\r\n", "OK", 500);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 3) {
                    goto _WIFI_RST;
                }
            }
            break;
        case 4:
            mutex = bsp_wifi_mutex_trylock(&g_wifi_drv, mutex);
            bsp_sta = bsp_wifi_cmd_get_ans(&g_wifi_drv, mutex, "AT+CIPMUX=1\r\n", "AT+CIPMUX=1", 1000);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta == BSP_OK) {
                    timeout_times = 0;
                    state++;
                } else if (bsp_sta == BSP_TIMEOUT) {
                    timeout_times++;
                }
                bsp_wifi_mutex_unlock(&g_wifi_drv, mutex);
                if (timeout_times >= 3) {
                    goto _WIFI_RST;
                }
            }
            break;
        default:
            state++;
            break;
        }
    }

    return;
_WIFI_RST:
    bsp_atbm6431_reset();
}
