/**
 * @file bsp_wifi_trans.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_WIFI_TRANS_H__
#define __BSP_WIFI_TRANS_H__

#include "stdint.h"
#include "bsp_types.h"

/* wifi传输驱动实例数据结构 */
typedef uint32_t wifi_mutex_desc;

enum WIFI_TRANS_TYPE {
    WIFI_NORMAL_TRANS = 0, /* TODO: trans to cmd_trans*/
    WIFI_UNVARNISHED_TRANS
};

enum WIFI_TRANS_STATE {
    WIFI_TRANS_IDLE = 0,
    WIFI_BUSY,
    WIFI_GET_CMPSTR,
    WIFI_GET_DATA,
    WIFI_GET_DONE,
    WIFI_GET_ERROR
};

struct wifi_driver_callbacks {
    void (*stream_handle_cb) (uint8_t byte);    /* tcp stream parser */
};

struct wifi_driver {
    enum WIFI_TRANS_TYPE    trans_type;
    enum WIFI_TRANS_STATE   trans_state;

    uint32_t cur_mutex;

    // uint32_t timeout;
    uint32_t last_tick;
    char *cmpstr;   /* 只读变量，除free()以外，严禁任何更改 */
    uint32_t cmpstr_window_bias;

    char *rxbuf;
    uint32_t rxcnt;

    struct wifi_driver_callbacks *wifi_drv_cbs;
};
/* wifi传输驱动实例数据结构 */

/* wifi缓冲区数据结构 */
struct tcp_message {
    struct list_head list;

    uint32_t len;
    uint8_t msg[];
};
/* wifi缓冲区数据结构 */

// /**
//  * @brief WIFI操作集
//  * 
//  */
// struct wifi_operations {
//     /* 启动透传 */
//     /* 关闭透传 */
//     /* 查询当前传输状态 */
//     /* 断开连接回调 */
//     /* WIFI热点连接(可阻塞)， 或返回成功，失败，忙三态 */
//     /* TCP连接建立(可阻塞) */
//     /* 数据发送(包含两种模式) */    /* 同一时间只能有一个通信通道，需要flag(互斥锁)对传输通道占用状态进行锁存 */
// };

void bsp_wifi_driver_init(struct wifi_driver *wifi_drv, uint32_t _ulBaud, struct wifi_driver_callbacks *cbs);
void bsp_wifi_byte_stream_isr(struct wifi_driver *wifi_drv);

void bsp_wifi_trans_reset(struct wifi_driver *wifi_drv);
BSP_StatusTypedef bsp_wifi_cmd_get_ans(struct wifi_driver *wifi_drv, uint32_t mutex, const char* cmd, const char* ans, uint32_t timeout);
BSP_StatusTypedef bsp_wifi_cmd_get_param(struct wifi_driver *wifi_drv, uint32_t mutex, const char* cmd, const char* param_head, char *buffer, uint32_t timeout);
BSP_StatusTypedef bsp_wifi_data_send(struct wifi_driver *wifi_drv, uint32_t mutex, const uint8_t *buffer, uint32_t len);
BSP_StatusTypedef bsp_wifi_start_unvarnished_txfer(struct wifi_driver *wifi_drv, uint32_t timeout);
BSP_StatusTypedef bsp_wifi_stop_unvarnished_txfer(struct wifi_driver *wifi_drv);

wifi_mutex_desc bsp_wifi_mutex_trylock(struct wifi_driver *wifi_drv, wifi_mutex_desc mutex);
void bsp_wifi_mutex_unlock(struct wifi_driver *wifi_drv, wifi_mutex_desc mutex);
uint32_t bsp_wifi_mutex_is_lock(struct wifi_driver *wifi_drv);

void bsp_wifi_atcmd_response_parser(struct wifi_driver *wifi_dev, const uint8_t _ucChar);

#endif /* __BSP_WIFI_TRANS_H__ */
