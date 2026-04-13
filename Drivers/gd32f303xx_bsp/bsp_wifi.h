/**
 * @file bsp_wifi.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_WIFI_H__
#define __BSP_WIFI_H__
#include <stdint.h>
#include "bsp_fifo.h"
#include "bsp_types.h"
#include "bsp_wifi_trans.h"

enum _wifi_core_state {
    WIFI_IDLE = 0,
    WIFI_GET_HOTSPOT_LIST,
    WIFI_CONNECTING,
    WIFI_WAIT_STABLE,
    WIFI_CONNECTED,
    WIFI_TCP_CONNECTING,
    WIFI_EVENT_HANDLE,
    WIFI_HP_DISCONNECTED,
    WIFI_TCP_DISCONNECTED,
    WIFI_CHECK_HP,
    WIFI_CHECK_TCP,
    WIFI_CHECK_RSSI
};

enum _wifi_connect_state {
    HP_DISCONNECTED = 0,
    HP_CONNECTED,
    TCP_CONNECTED
};

/**
 * @brief 普通数据传输模式；透传模式
 * 
 */
struct wifi_device {
    struct wifi_driver *wifi_drv;
    enum _wifi_core_state wifi_core_cur_state;
    enum _wifi_connect_state connect_state;

    /* TODO: 业务层结构体需要进一步进行封装，或转移至业务层头文件中声明 */
    struct {
        uint8_t refresh_hp:1;
        uint8_t hp_connect:1;
        uint8_t tcp_connect:1;
        uint8_t hp_disconnect:1;
        uint8_t tcp_disconnect:1;
        uint8_t tcp_send_msg:1;
    } req;
    
    struct {
        uint8_t hp_list_valid:1;
        uint8_t rssi_valid:1;
    } flag;

    char *hp_connect_str;
    char *tcp_connect_str;

    /* tcp软连接通信 */
    uint8_t tcp_lid2send;
    struct tcp_message *tcp_msg2send; /*待发送的数据 */
};

/**
 * @brief wifi热点信息链表结构体
 * 
 */
struct _wifi_object {
    struct list_head list;
    uint8_t num;
    char ssid[32];  /* 热点ssid*/
    char rssi[8];   /* 热点信号强度 */
    char bssid[18]; /* 热点bssid<暂时不用> */
};

/**
 * @brief 供用户保存wifi热点及密码的链表结构体示例
 * 
 */
struct _wifi_password {
    struct list_head list;
    char ssid[32];
    char bssid[6];
    char password[64];
};

void bsp_wifi_dev_init(struct wifi_device *wifi_dev, struct wifi_driver *wifi_drv);
// void bsp_wifi_application_test(struct wifi_device *_wifi_dev);
void bsp_wifi_sta_task(struct wifi_device *_wifi_dev);
BSP_StatusTypedef wifi_refresh_hotspot_list(struct wifi_device *_wifi_dev);
struct _wifi_object* bsp_wifi_get_ssid_list(struct wifi_device *_wifi_dev);
BSP_StatusTypedef bsp_wifi_connect_hotspot(struct wifi_device *_wifi_dev, char *ssid, char *pwd);
BSP_StatusTypedef bsp_wifi_disconnect_tcp(struct wifi_device *_wifi_dev);
BSP_StatusTypedef bsp_wifi_disconnect_hp(struct wifi_device *_wifi_dev);
uint8_t bsp_wifi_get_rssi(struct wifi_device *_wifi_dev);
/* tcp通信操作函数 */
BSP_StatusTypedef bsp_wifi_connect_tcp(struct wifi_device *_wifi_dev, uint8_t pid, char *ipv4_addr, uint16_t port);
void bsp_wifi_tcp_message_parser(struct wifi_device *wifi_dev, const uint8_t _ucChar);
BSP_StatusTypedef bsp_wifi_tcp_send_request(struct wifi_device *wifi_dev, uint8_t link_id, const uint8_t *buffer, uint32_t len);
uint32_t bsp_wifi_tcp_get_msg(struct wifi_device *_wifi_dev, uint8_t link_id, uint8_t *buffer, uint32_t len);

/**
 * @brief 获取当前wifi station状态机状态
 * 
 * @return enum _wifi_core_state 
 */
static inline enum _wifi_core_state bsp_wifi_get_core_state(struct wifi_device *_wifi_dev) {
    return _wifi_dev->wifi_core_cur_state;
}

static inline enum _wifi_connect_state bsp_wifi_get_connect_state(struct wifi_device *_wifi_dev) {
    return _wifi_dev->connect_state;
}

#endif /* __BSP_WIFI_H__ */
