/**
 * @file bsp_atbm6431.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2025-07-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_ATBM6431_H__
#define __BSP_ATBM6431_H__
#include "stdint.h"
#include "bsp_types.h"
#include "bsp_wifi.h"
#include "bsp_wifi_trans.h"
extern struct wifi_device g_wifi_dev;
extern struct wifi_driver g_wifi_drv;

void bsp_atbm6431_init(uint32_t ulBaud);
void bsp_atbm6431_reset(void);
void bsp_atbm6431_enter_sleepmode(void);
void bsp_atbm6431_exit_sleepmode(void);

static inline void bsp_atbm6431_isr(void) {
    bsp_wifi_byte_stream_isr(&g_wifi_drv);
}

static inline void bsp_atbm6431_task_handler(void) {
	bsp_wifi_sta_task(&g_wifi_dev);
}

static inline BSP_StatusTypedef bsp_atbm6431_send_message(const uint8_t *buffer, uint32_t len) {
    return bsp_wifi_tcp_send_request(&g_wifi_dev, 0, buffer, len);
}

static inline uint32_t bsp_atbm6431_get_message(uint8_t *buffer, uint32_t len) {
    return bsp_wifi_tcp_get_msg(&g_wifi_dev, 0, buffer, len);
}

static inline void bsp_atbm6431_rst_trans_chan(void) {
    // __NOP();
}

static inline enum _wifi_core_state bsp_atbm6431_get_core_state(void) {
    return bsp_wifi_get_core_state(&g_wifi_dev);
}

static inline enum _wifi_connect_state bsp_atbm6431_get_connect_state(void) {
    return bsp_wifi_get_connect_state(&g_wifi_dev);
}

static inline BSP_StatusTypedef bsp_atbm6431_connect_hotspot(char *ssid, char *pwd) {
    return bsp_wifi_connect_hotspot(&g_wifi_dev, ssid, pwd);
}

static inline BSP_StatusTypedef bsp_atbm6431_connect_tcp(uint8_t pid, char *ipv4_addr, uint16_t port) {
    return bsp_wifi_connect_tcp(&g_wifi_dev, pid, ipv4_addr, port);
}

static inline BSP_StatusTypedef bsp_atbm6431_disconnect_hp(void) {
    return bsp_wifi_disconnect_hp(&g_wifi_dev);
}

static inline BSP_StatusTypedef bsp_atbm6431_disconnect_tcp(void) {
    return bsp_wifi_disconnect_tcp(&g_wifi_dev);
}

static inline uint8_t bsp_atbm6431_get_rssi(void) {
    return bsp_wifi_get_rssi(&g_wifi_dev);
}

static inline BSP_StatusTypedef bsp_atbm6431_refresh_hp(void) {
    return wifi_refresh_hotspot_list(&g_wifi_dev);
}

static inline struct _wifi_object*  bsp_atbm6431_get_ssid_list(void) {
	return bsp_wifi_get_ssid_list(&g_wifi_dev);
}

#endif /* __BSP_ATBM6431_H__ */
