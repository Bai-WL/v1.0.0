/**
 * @file bsp_wifi.c
 * @author your name (you@domain.com)
 * @brief wifi模块业务层封装
 * @version 0.7
 * @date 2025-07-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "gd32f30x.h"
#include "bsp_fifo.h"
#include "bsp_malloc.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_wifi.h"
#include "bsp_wifi_trans.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_assert.h"

#define IPD_FIFO_SIZE 1024
static struct bsp_fifo ipd_fifo_0;
static uint8_t ipd_buffer_0[IPD_FIFO_SIZE];
static struct _wifi_object inquired_wifi_list = {
	.list = {
		.next = NULL,
		.prev = NULL
	}
};
static uint8_t wifi_rssi = 0;
static BSP_StatusTypedef tcp_send_request_handler(struct wifi_driver *wifi_drv, uint32_t mutex, uint8_t link_id, const uint8_t *buffer, uint32_t len);
char g_tcp_connect_str[64] = "AT+CIPSTART=0,\"TCP\",\"10.5.5.1\",502\r\n";
char g_hp_cnt_str[64] = "AT+CWJAP_CUR=\"HMI-710164-88129\",\"\"\r\n";
void bsp_wifi_dev_init(struct wifi_device *wifi_dev, struct wifi_driver *wifi_drv)
{
    struct list_head *pos, *n;
    uint32_t mutex = 0;
    bsp_fifo_init(&ipd_fifo_0, ipd_buffer_0, IPD_FIFO_SIZE);

    wifi_dev->wifi_drv = wifi_drv;
    /* 业务层 */
    wifi_dev->wifi_core_cur_state = WIFI_IDLE;
    wifi_dev->connect_state = HP_DISCONNECTED;
    wifi_dev->req.refresh_hp = 0;
    wifi_dev->req.hp_connect = 0;
    wifi_dev->req.tcp_connect = 0;
    wifi_dev->req.hp_disconnect = 0;
    wifi_dev->req.tcp_disconnect = 0;
    wifi_dev->req.tcp_send_msg = 0;
    wifi_dev->flag.hp_list_valid = 0;
    wifi_dev->flag.rssi_valid = 0;
    wifi_dev->hp_connect_str = g_hp_cnt_str;
    wifi_dev->tcp_connect_str = g_tcp_connect_str;

    wifi_dev->tcp_lid2send = 0;
    wifi_dev->tcp_msg2send = NULL;

    mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+GET_VER\r\n", NULL, 100));
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+GET_SDK_VER\r\n", NULL, 100));
    while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+MODEM_SLEEP 1\r\n", "OK", 300));
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWMODE_DEF=1\r\n", "OK", 100));  /* TODO: 20250730 实现自动识别配置 */
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWQAP\r\n", "OK", 100));
    while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWLAPOPT,1,6\r\n", "OK", 500));  /* 只获取ssid和信号强度，但是会存在一个问题，ssid会重名 */ /* TODO:实现删除重复ssid的程序*/
    while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CIPMUX=1\r\n", "OK", 1000)); /* 配置为多连接 */
    bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);

    if (inquired_wifi_list.list.next != NULL) {
        list_for_each_safe(pos, n, &inquired_wifi_list.list) {  /* 清除热点链表 */
            bsp_assert(pos != &inquired_wifi_list.list);
            list_del(pos);
            bsp_free(pos);
        }
    }
    INIT_LIST_HEAD(&inquired_wifi_list.list);
}

void bsp_wifi_core_deinit(struct wifi_device *wifi_dev, uint32_t mutex)
{
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+RST\r\n", "OK", 2000));  /* 当wifi芯片处于AP模式时，复位需要很长时间，因此此处复位时间设置为2s */
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+DBG_CLOSE\r\n", "OK", 10));
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "ATE0\r\n", "OK", 10));
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWMODE_CUR=1\r\n", "OK", 100));
	while (bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWQAP\r\n", "OK", 100));
}

/**
 * @brief 获取wifi热点信号强度。
 *          注：获取信号强度前，建议先对wifi状态机的当前状态进行判断
 *              信号强度并不实时, 理想条件下每2秒刷新一次。
 * 
 * @return uint8_t 信号强度，单位dB 
 *                 当返回值为0时，表示该数据无效
 */
uint8_t bsp_wifi_get_rssi(struct wifi_device *wifi_dev)
{
	if (!wifi_dev->flag.rssi_valid)
		return 0;
	
    if (wifi_dev->wifi_core_cur_state < WIFI_CONNECTED) {
		wifi_dev->flag.rssi_valid = 0;
		return 0;
	}

    if (wifi_dev->flag.rssi_valid) 
        return wifi_rssi;

    return 0;
}

/**
 * @brief 刷新wifi热点列表
 * 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef wifi_refresh_hotspot_list(struct wifi_device *wifi_dev)
{
    if (wifi_dev->wifi_core_cur_state != WIFI_IDLE)
        return BSP_ERROR;

    wifi_dev->flag.hp_list_valid = 0;
    wifi_dev->req.refresh_hp = 1;
    return BSP_OK;
}

/**
 * @brief 获取wifi热点链表
 * 
 * @return struct _wifi_object* 
 */
struct _wifi_object* bsp_wifi_get_ssid_list(struct wifi_device *wifi_dev)
{
    if (wifi_dev->flag.hp_list_valid)
        return &inquired_wifi_list;
    else 
        return NULL;
}

/**
 * @brief 连接wifi热点
 * 
 * @param ssid ssid字符串必须带上前后的\"，以及结束符\0
 * @param pwd 密码的字符串必须带上前后的\"，以及结束符\0
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_connect_hotspot(struct wifi_device *wifi_dev, char *ssid, char *pwd)
{
    if (wifi_dev->wifi_core_cur_state != WIFI_IDLE)
        return BSP_ERROR;
    strcpy(wifi_dev->hp_connect_str, "AT+CWJAP_DEF=");
    strcat(wifi_dev->hp_connect_str, ssid);
    strcat(wifi_dev->hp_connect_str, ",");
    strcat(wifi_dev->hp_connect_str, pwd);
    strcat(wifi_dev->hp_connect_str, "\r\n");
	wifi_dev->req.hp_connect = 1;
    
    return BSP_OK;
}

/**
 * @brief 请求连接tcp
 * 
 * @param pid 默认填0
 * @param ipv4_addr 传入参数例: "\"10.5.5.1\""，必须带结束符\0
 * @param port 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_connect_tcp(struct wifi_device *wifi_dev, uint8_t pid, char *ipv4_addr, uint16_t port)
{
    char tmpChar[32];
    if (wifi_dev->wifi_core_cur_state != WIFI_CONNECTED) 
        return BSP_ERROR;
    sprintf(wifi_dev->tcp_connect_str, "AT+CIPSTART=%d,\"TCP\",", pid);
    strcat(wifi_dev->tcp_connect_str, ipv4_addr);
    sprintf(tmpChar, ",%d\r\n", port);
    strcat(wifi_dev->tcp_connect_str, tmpChar);
	wifi_dev->req.tcp_connect = 1;
	return BSP_OK;
}

/**
 * @brief 断开热点连接
 * 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_disconnect_hp(struct wifi_device *wifi_dev)
{
    if (wifi_dev->wifi_core_cur_state != WIFI_EVENT_HANDLE && wifi_dev->wifi_core_cur_state != WIFI_CONNECTED) 
        return BSP_ERROR;
    wifi_dev->req.hp_disconnect = 1;
    return BSP_OK;
}

/**
 * @brief 断开tcp连接
 * 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_disconnect_tcp(struct wifi_device *wifi_dev)
{
    if (wifi_dev->wifi_core_cur_state != WIFI_EVENT_HANDLE) 
        return BSP_ERROR;
    wifi_dev->req.tcp_disconnect = 1;
    return BSP_OK;
}

uint8_t g_rxbuf[1024] = {0};
/**
 * @brief wifi station核心状态机
 * 
 */
void bsp_wifi_sta_task(struct wifi_device *wifi_dev)
{
    static uint32_t mutex = 0;
    static uint8_t timeout_cnt = 0;
    static enum _wifi_core_state where_disconnect_from = WIFI_IDLE;
    static enum _wifi_core_state where_checkconnect_from = WIFI_IDLE;
    uint8_t *pc;
    struct _wifi_object *new_obj;
    struct list_head *pos, *n;
    uint32_t i, j;
    BSP_StatusTypedef bsp_sta = BSP_ERROR;
	static uint32_t last_tick = 0;
    static uint32_t last_tick_check_connect = 0;

    switch (wifi_dev->wifi_core_cur_state) {
    case WIFI_IDLE:
        wifi_dev->flag.rssi_valid = 0;
        if (wifi_dev->req.hp_connect) {	/* 优先响应连接 */
            wifi_dev->wifi_core_cur_state = WIFI_CONNECTING;
            wifi_dev->req.hp_connect = 0; 
        } else if (wifi_dev->req.refresh_hp) {
            wifi_dev->wifi_core_cur_state = WIFI_GET_HOTSPOT_LIST;
            wifi_dev->req.refresh_hp = 0;
        }
		break;
    case WIFI_GET_HOTSPOT_LIST:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_param(wifi_dev->wifi_drv, mutex, "AT+CWLAP\r\n", "+CWLAP:", (char *)g_rxbuf, 3000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                pc = g_rxbuf;
                g_rxbuf[511] = '\0';
                list_for_each_safe(pos, n, &inquired_wifi_list.list) {  /* 清除热点链表 */
                    bsp_assert(pos != &inquired_wifi_list.list);
                    list_del(pos);
                    bsp_free(pos);
                }

                for (i = 0; i < 7; ) {  /* 请求热点过多，会导致爆堆 */ /* TODO:当wifi热点过少时，需要有终止信号 */
                    while (*pc != '(') {
						if (*pc == '\0')
							break;
						pc++;
					}
					if (*pc == '\0') 
						break;
					pc++;
					
                    new_obj = (struct _wifi_object *)bsp_malloc(sizeof(struct _wifi_object));
                    if (new_obj == NULL) {
						break;
                    }
                    for (j = 0; (*pc != ',') && (*pc != '\0'); j++, pc++) {	/* FIXME:当名字中存在','或')'，便会出错! */
                        new_obj->ssid[j] = *pc;
                    }

                    if (*pc != '\0') {
                        new_obj->ssid[j] = '\0';
                        pc++;
                    }
                    for (j = 0; (*pc != ')') && (*pc != '\0'); j++, pc++) {
                        new_obj->rssi[j] = *pc;
                    }
                    if (*pc != '\0') {  /* 解决扫描信息异常的问题, (\" " \") */
                        new_obj->rssi[j] = '\0';
                        list_for_each_safe(pos, n, &inquired_wifi_list.list) { /* 查询是否存在同名热点 */
                            if (!strcmp(((struct _wifi_object *)pos)->ssid, new_obj->ssid)) {
                                break;
                            }
                        }

                        for (j = 0; new_obj->ssid[j] != '\0' && new_obj->ssid[j] < 0x80; j++) ;

                        if (new_obj->ssid[j] < 0x80) {  /* 检查是否存在中文字符 */
                            if (pos == &inquired_wifi_list.list) {
                                list_add_tail(&new_obj->list, &inquired_wifi_list.list);
                                i++;
                            } else {
                                bsp_free(new_obj);
                            }
                        } else {
                            bsp_free(new_obj);
                        }
                    } else {
                        bsp_free(new_obj);
                    }
                }
                /* TODO: 遍历链表删除重复元素 */
                wifi_dev->flag.hp_list_valid = 1;

                wifi_dev->wifi_core_cur_state = WIFI_IDLE;
            } else if (bsp_sta == BSP_TIMEOUT) {
                wifi_dev->flag.hp_list_valid = 0;
                wifi_dev->wifi_core_cur_state = WIFI_IDLE;
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_CONNECTING:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, wifi_dev->hp_connect_str, "GOT IP\r\n\r\nOK", 20000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                wifi_dev->wifi_core_cur_state = WIFI_WAIT_STABLE;
				last_tick = bsp_get_systick();
            } else if (bsp_sta == BSP_TIMEOUT) {
                wifi_dev->wifi_core_cur_state = WIFI_IDLE;
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_WAIT_STABLE:
		if ((bsp_get_systick() - last_tick) > 250) {	/* wifi芯片稳定时间 */
            wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
            wifi_dev->connect_state = HP_CONNECTED;
        } else {
            break;
        }
    case WIFI_CONNECTED:
        if ((bsp_get_systick() - last_tick_check_connect) > 2000) {
            wifi_dev->flag.rssi_valid = 0;
            where_checkconnect_from = WIFI_CHECK_HP;
            wifi_dev->wifi_core_cur_state = WIFI_CHECK_RSSI;
            break;
        }
        if (wifi_dev->req.hp_disconnect) {
            wifi_dev->wifi_core_cur_state = WIFI_HP_DISCONNECTED;
            break;
        }
        if (wifi_dev->req.tcp_connect) {
                /* wifi连接成功后，需要时间来稳定 */
                wifi_dev->wifi_core_cur_state = WIFI_TCP_CONNECTING;
                wifi_dev->req.tcp_connect = 0;
        } else {
            break;
        }
    case WIFI_TCP_CONNECTING:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, wifi_dev->tcp_connect_str, "CONNECT", 10000); 
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                wifi_dev->wifi_core_cur_state = WIFI_EVENT_HANDLE;
                wifi_dev->connect_state = TCP_CONNECTED;
            } else if (bsp_sta == BSP_TIMEOUT) {
                wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break; 
    case WIFI_EVENT_HANDLE:
        if (wifi_dev->req.tcp_send_msg) {
            mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
            bsp_sta = tcp_send_request_handler(wifi_dev->wifi_drv, mutex, wifi_dev->tcp_lid2send, wifi_dev->tcp_msg2send->msg, wifi_dev->tcp_msg2send->len);
            if (bsp_sta != BSP_BUSY) {
                if (bsp_sta != BSP_OK) {
                    __NOP();
                }
                /* 不对发送是否成功进行判断 */
                wifi_dev->req.tcp_send_msg = 0;
                bsp_free(wifi_dev->tcp_msg2send);
                bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
            }
        }
        if (bsp_wifi_mutex_is_lock(wifi_dev->wifi_drv)) {
            break;
        }
        if ((bsp_get_systick() - last_tick_check_connect) > 2000) {
            wifi_dev->flag.rssi_valid = 0;
            where_checkconnect_from = WIFI_CHECK_TCP;
            wifi_dev->wifi_core_cur_state = WIFI_CHECK_RSSI;
        } else if (wifi_dev->req.hp_disconnect) {
            where_disconnect_from = WIFI_EVENT_HANDLE;
            wifi_dev->wifi_core_cur_state = WIFI_HP_DISCONNECTED;
        } else if (wifi_dev->req.tcp_disconnect) {
            where_disconnect_from = WIFI_EVENT_HANDLE;
            wifi_dev->wifi_core_cur_state = WIFI_TCP_DISCONNECTED;
        } 
        break;
    case WIFI_HP_DISCONNECTED:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CWQAP\r\n", "OK", 3000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                wifi_dev->wifi_core_cur_state = WIFI_IDLE;
                wifi_dev->connect_state = HP_DISCONNECTED;
            } else if (bsp_sta == BSP_TIMEOUT) {
                wifi_dev->wifi_core_cur_state = where_disconnect_from;
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            /* 复位WIFI状态 */
            wifi_dev->req.hp_disconnect = 0;
            wifi_dev->req.tcp_disconnect = 0;
            wifi_dev->req.hp_connect = 0;
            wifi_dev->req.tcp_connect = 0;
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_TCP_DISCONNECTED:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_ans(wifi_dev->wifi_drv, mutex, "AT+CIPCLOSE=0\r\n", "OK", 3000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
                wifi_dev->connect_state = HP_CONNECTED;
            } else if (bsp_sta == BSP_TIMEOUT) {
                wifi_dev->wifi_core_cur_state = where_disconnect_from;
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            /* 复位WIFI状态 */
            wifi_dev->req.hp_disconnect = 0;
            wifi_dev->req.tcp_disconnect = 0;
            wifi_dev->req.hp_connect = 0;
            wifi_dev->req.tcp_connect = 0;
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_CHECK_HP:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_param(wifi_dev->wifi_drv, mutex, "AT+CIPSTATUS\r\n", "STATUS:", (char *)g_rxbuf, 1000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR)
                break;

            if (bsp_sta == BSP_OK) {
                timeout_cnt = 0;
                switch (g_rxbuf[0]) {
                case '2': /* 已连接AP */
                case '3': /* 已建立TCP */
                case '4': /* 断开网络 */
                    wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
                    break;
                case '5': /* 断开AP */
                    wifi_dev->wifi_core_cur_state = WIFI_IDLE;
                    wifi_dev->connect_state = HP_DISCONNECTED;
                    break;
                default:
                    wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
                    break;
                }
            } else if (bsp_sta == BSP_TIMEOUT) {
                if (++timeout_cnt > 10) {
                    wifi_dev->wifi_core_cur_state = WIFI_IDLE;
                    timeout_cnt = 0;
                }
            }
            last_tick_check_connect = bsp_get_systick();
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_CHECK_TCP:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_param(wifi_dev->wifi_drv, mutex, "AT+CIPSTATUS\r\n", "STATUS:", (char *)g_rxbuf, 1000);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_ERROR)
                break;

            if (bsp_sta == BSP_OK) {
                timeout_cnt = 0;
                switch (g_rxbuf[0]) {
                case '3': /* 已建立TCP */
                    wifi_dev->wifi_core_cur_state = WIFI_EVENT_HANDLE;
                    break;
                case '5': /* 断开AP */
                    wifi_dev->wifi_core_cur_state = WIFI_IDLE;
                    wifi_dev->connect_state = HP_DISCONNECTED;
                    break;
                case '2': /* 已连接AP */
                case '4': /* 断开网络 */
                    wifi_dev->wifi_core_cur_state = WIFI_CONNECTED;
                    wifi_dev->connect_state = HP_CONNECTED;
                    break;
                default:
                    wifi_dev->wifi_core_cur_state = WIFI_EVENT_HANDLE;
                    break;
                }
            } else if (bsp_sta == BSP_TIMEOUT) {
                if (++timeout_cnt > 10) {
                    wifi_dev->wifi_core_cur_state = WIFI_IDLE;
                    timeout_cnt = 0;
                }
            }
            last_tick_check_connect = bsp_get_systick();
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    case WIFI_CHECK_RSSI:
        mutex = bsp_wifi_mutex_trylock(wifi_dev->wifi_drv, mutex);
        bsp_sta = bsp_wifi_cmd_get_param(wifi_dev->wifi_drv, mutex, "AT+CWJAP_CUR?\r\n", "+CWJAP_CUR:", (char *)g_rxbuf, 100);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                pc = g_rxbuf;
                while (*pc != '\0' && *pc != ',') pc++;
                while (*pc != '\0' && *pc != '-') pc++;
                if (*pc == '-') {
                    wifi_rssi = (*(pc + 1) - '0') * 10 + (*(pc + 2) - '0');
                    wifi_dev->flag.rssi_valid = 1;
                } else {
                    wifi_dev->flag.rssi_valid = 0;
                }
            } else if (bsp_sta == BSP_TIMEOUT) {
                __NOP();
            } else if (bsp_sta == BSP_ERROR) {
                break;
            }
            last_tick_check_connect = bsp_get_systick();
            wifi_dev->wifi_core_cur_state = where_checkconnect_from;
            bsp_wifi_mutex_unlock(wifi_dev->wifi_drv, mutex);
        }
        break;
    default :  
		break;
    }
}

/********** tcp软连接通信处理 **********/
uint32_t ipd_data_err_cnt = 0;
uint32_t ipd_len_err_cnt = 0;
void bsp_wifi_tcp_message_parser(struct wifi_device *wifi_dev, const uint8_t _ucChar)
{
    static enum {
        IPD_IDLE = 0,
        IPD_GET_LINK_ID,
        IPD_GET_LEN,
        IPD_GET_DATA,
    } state = IPD_IDLE;
    static uint8_t WifiIpdHead[] = "+IPD,0,";
    static uint8_t pHeadBias = 0;
    static uint8_t tmpbuf[8];
    static uint32_t DataLen = 0;
    static uint32_t recv_cnt = 0;

    /* 持续捕获"+IPD"报文头 */ /* TODO: 找到更好的错误传输处理方案。当前方案无法处理TCP报文中的+IPD数据段 */
    if (_ucChar == WifiIpdHead[pHeadBias]) {
        pHeadBias++;
    } else {
        pHeadBias = 0;
        if (_ucChar == WifiIpdHead[pHeadBias]) {
            pHeadBias++;
        }
    }
    /* IPD 完全匹配 */
    if (WifiIpdHead[pHeadBias] == '\0') {
        if (recv_cnt != DataLen) { /* IPD接收异常 */
            ipd_data_err_cnt++;
        }
        /* 重置所有传输状态 */
        if (wifi_dev->connect_state == TCP_CONNECTED) {
			DataLen = 0;
			recv_cnt = 0;
			state = IPD_GET_LEN;
        } else {
            state = IPD_IDLE;
        }
        return ;
    } 

    switch (state) {
    case IPD_IDLE:
        break;
    case IPD_GET_LINK_ID:   /* 确认Link ID合法性 */
        if (_ucChar != ',') {
            if (_ucChar <= '3' && _ucChar >= '0') {

            } else {

            }
        }
        break;
    case IPD_GET_LEN:
        if (_ucChar != ':') {
            if (_ucChar <= '9' && _ucChar >= '0') {
                DataLen = DataLen * 10 + _ucChar - '0';
            } else {
                ipd_len_err_cnt++;
                state = IPD_IDLE;
            }
        } else {
            if (DataLen < 256) {
                state = IPD_GET_DATA;
			} else {
                ipd_len_err_cnt++;
                state = IPD_IDLE;
            }
        }
        break;
    case IPD_GET_DATA:
        tmpbuf[recv_cnt & 0x7] = _ucChar;
        recv_cnt++;
        if (recv_cnt == DataLen) {
            bsp_fifo_put(&ipd_fifo_0, tmpbuf, ((recv_cnt - 1) & 0x07) + 1);
            state = IPD_IDLE;
            break;
        } else if (((recv_cnt - 1) & 0x7) == 0x7) { /* 每8存1 */
            bsp_fifo_put(&ipd_fifo_0, tmpbuf, 8);
        }
        break;
    default:
        break;
    }
}

uint32_t bsp_wifi_tcp_fifo_len(struct wifi_device *wifi_dev, uint8_t link_id)
{
    switch (link_id) {
    case 0:
        return bsp_fifo_len(&ipd_fifo_0);
    case 1:
        break;
    case 2:
        break;
    case 3:
        break;
    default:
        break;  
    }

    return 0;
}

/**
 * @brief 
 * 
 * @param link_id 
 * @return uint32_t 报文长度
 */
uint32_t bsp_wifi_tcp_get_msg(struct wifi_device *wifi_dev, uint8_t link_id, uint8_t *buffer, uint32_t len)
{
    switch (link_id) {
    case 0:
        return bsp_fifo_get(&ipd_fifo_0, buffer, len);
    case 1:
        break;
    case 2:
        break;
    case 3:
        break;
    default:
        break;  
    }

    return 0;
}

/**
 * @brief 建立tcp发送请求
 * 
 * @param wifi_dev 
 * @param link_id 
 * @param buffer 
 * @param len 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_wifi_tcp_send_request(struct wifi_device *wifi_dev, uint8_t link_id, const uint8_t *buffer, uint32_t len)
{
    struct tcp_message *p_msg = NULL;
    if (wifi_dev->req.tcp_send_msg)  /* 存在其他的tcp发送请求 */
        return BSP_BUSY;
    
    if (wifi_dev->connect_state != TCP_CONNECTED) 
        return BSP_ERROR;
    
    p_msg = (struct tcp_message *)bsp_malloc(sizeof(struct tcp_message) + len);
    if (p_msg != NULL) {
        wifi_dev->tcp_lid2send = link_id;
        memcpy(p_msg->msg, buffer, len);
        p_msg->len = len;
        wifi_dev->tcp_msg2send = p_msg;
    } else {
        return BSP_ERROR;
    }
    wifi_dev->req.tcp_send_msg = 1;

    return BSP_OK;
}

/**
 * @brief 该函数非独立对象，为wifi状态机的逻辑子函数，不是通信函数
 *        当返回值不为BSP_BUSY时，说明函数执行完成，函数状态重置, 需对mutex解锁 
 * @param wifi_drv 
 * @param mutex 
 * @param link_id 
 * @param buffer 
 * @param len 
 * @return BSP_StatusTypedef 
 */
#define BUFF_LEN_LMT 256
uint8_t tcp_send_buffer[BUFF_LEN_LMT];
static uint32_t slen = 0;
static BSP_StatusTypedef tcp_send_request_handler(struct wifi_driver *wifi_drv, uint32_t mutex, uint8_t link_id, const uint8_t *buffer, uint32_t len)
{
    static uint8_t wifi_tcp_send_sta = 0;   
    BSP_StatusTypedef bsp_sta = BSP_BUSY;

    switch (wifi_tcp_send_sta) {    /* TODO: 将该状态变量命名更改为wifi_dma_send_sta */
    case 0:
        if (mutex) {
            slen = sprintf((char*)tcp_send_buffer, "AT+SOCKETFASTSEND=%d,%d\r\n", link_id, len);
            if (slen + len + 2 > BUFF_LEN_LMT) 
                return BSP_ERROR;   /* reject message sending  */

			memcpy(tcp_send_buffer + slen, buffer, len);
			slen += len;
			memcpy(tcp_send_buffer + slen, "\r\n", 2);
			slen += 2;
            wifi_tcp_send_sta = 1;
        } else {
            break;
        }
    case 1: /* 发送待传输的tcp数据包 */
        bsp_sta = bsp_wifi_data_send(wifi_drv, mutex, tcp_send_buffer, slen);
        if (bsp_sta != BSP_BUSY) {
            wifi_tcp_send_sta = 0;
            return bsp_sta;
        } 
        break;
    default:
        break;
    }

    return BSP_BUSY;
}

/**
 * @brief wifi tcp业务注册
 *        传入业务回调函数，业务执行过程中，需使用一标志位避免状态机的转移。
 */
// void bsp_wifi_application_register(void)
// {

// }

///**
// * @brief 热点列表获取及显示示例
// * 
// */
//void bsp_wifi_application_test(struct wifi_device *wifi_dev)
//{   
//    static uint8_t hp_list_show_sta = 0;
//    static uint32_t last_tick = 0;
//    struct _wifi_object *hp_list;
//    struct list_head *pos;
//    uint32_t i = 0;
//    switch (hp_list_show_sta) {
//    case 0:
//        if (wifi_refresh_hotspot_list() == BSP_OK) {
//            hp_list_show_sta = 1;
//        }
//        break;
//    case 1:
//        hp_list = bsp_wifi_get_ssid_list();
//        if (hp_list != NULL) {
//            bsp_JLXLcdClearLine(2, 14, 0x00);
//            list_for_each(pos, &(hp_list->list)) {
//                if (i >= 7) {
//                    break;
//                }
//                bsp_JLXLcdShowString(8, (i+1)*2, ((struct _wifi_object*)pos)->ssid, 0, 0);
//                i++;
//            }
//            hp_list_show_sta = 2;
//        } else if (wifi_dev->wifi_core_cur_state != WIFI_GET_HOTSPOT_LIST) {
//            hp_list_show_sta = 2;
//        }
//        last_tick = bsp_get_systick();
//        break;
//    case 2:
//        if ((bsp_get_systick() - last_tick) > 5000) {
//            hp_list_show_sta = 0;
//        }
//        break;
//    default:
//        break;
//    }
//}
