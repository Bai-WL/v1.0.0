/**
 * @file bsp_modbus_tcp.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.4
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <string.h>
#include <stdlib.h>
#include "bsp_malloc.h"
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_modbus_tcp.h"
#include "bsp_types.h"
#include "bsp_fifo.h"
#include "bsp_gd32f303xx_systick.h"
#include "gd32f30x.h"
#include "bsp_assert.h"

/* private datatype */
struct req_item {
    struct list_head list;
    uint16_t tid; /* modbus-tcp 事务标识符 */
    uint8_t tx_flag;
    /* 仅调试时使用20250510 */
    uint32_t tx_len;
    uint8_t tx_buffer[];
    /* 仅调试时使用20250510 */
};

struct rsp_item {
    struct list_head list;
    uint16_t tid;
    BSP_ModbusErrorTypeDef err_code;
    uint32_t rx_dlen;
    uint16_t rx_data[];
};
/* private datatype */

struct list_head rsp_window;    /* 数据缓冲区 */
struct list_head req_window;    /* 请求列表窗口 */
void bsp_modbus_tcp_window_init(void)
{
	INIT_LIST_HEAD(&rsp_window);
	INIT_LIST_HEAD(&req_window);
}

BSP_StatusTypedef bsp_modbus_tcp_submit_request(BSP_ModbusHandleTypedef *_hModbus,
                                                uint8_t link_id,
                                                uint16_t *tid,
                                                uint8_t _ucSlaveAddr,
                                                uint8_t _ucCmd, 
                                                uint16_t _ucRegAddr, 
                                                uint16_t *_psData, 
                                                uint8_t _ucDataLen)
{
    uint8_t tmpbuf[300];
    struct req_item *p_req = NULL;
    struct list_head *pos, *n;
    uint8_t uctmp = 0;
	int i = 0;

    *tid = 0;
    /* 检查缓冲区是否空余 */
    list_for_each_safe(pos, n, &req_window) {
        uctmp++;
    }
    if (uctmp >= REQ_WINDOW_SIZE)
        return BSP_BUSY;

    bsp_assert(_hModbus->Protocol == MB_TCP);

    /* 检查传入参数合法性 */
    for (i = 0; i < _hModbus->CmdLen; i++) {
        if (_ucCmd == _hModbus->pcCmd[i]) {
            _hModbus->CmdIndex = i;
			break;
        }
    }
	if (i >= _hModbus->CmdLen) {
		return BSP_ERROR;
	}

    /* 添加MODBUS_TCP专属报文头 */
    _hModbus->pcTxBuffer = tmpbuf + 6;
    tmpbuf[0] = (_hModbus->trans_id>>8) & 0xFF;
    tmpbuf[1] = _hModbus->trans_id & 0xFF;
    tmpbuf[2] = (_hModbus->pid>>8) & 0xFF;
    tmpbuf[3] = _hModbus->pid & 0xFF;

    /* 向报文生成代码提供从机地址及寄存器地址信息 */
    _hModbus->SlaveAddr = _ucSlaveAddr;
    _hModbus->pcTxBuffer[0] = (_ucRegAddr>>8) & 0xFF;
    _hModbus->pcTxBuffer[1] = _ucRegAddr & 0xFF;
    /* 调用modbus报文生成代码生成报文 */
    if (_ucDataLen) {
        _hModbus->DataByteLen = _ucDataLen*2;
        _hModbus->pcDataBuffer = (uint8_t *) _psData;
    }
    if (_hModbus->eCmdCallback[_hModbus->CmdIndex](_hModbus)) 
        return BSP_ERROR;   /* TODO: 看这个地方会不会进去，如果会，则此处为内存泄漏的根本原因 */

    /* 对PDU数据段长度进行计算，并添加至TCP报文头中 */
    _hModbus->msg_len = _hModbus->TxCnt;
    tmpbuf[4] = (_hModbus->msg_len>>8) &0xFF;
    tmpbuf[5] = _hModbus->msg_len & 0xFF;
    /* 保存报文至待发送缓冲区 */
    p_req = (struct req_item*)bsp_malloc(sizeof(struct req_item) + _hModbus->msg_len + 6);
    if (p_req) {
        p_req->tid = _hModbus->trans_id;
        p_req->tx_flag = 0;
        p_req->tx_len = _hModbus->msg_len + 6;
        memcpy(p_req->tx_buffer, tmpbuf, p_req->tx_len);
        list_add_tail(&p_req->list, &req_window);
    } else {
        return BSP_ERROR;
    }
    *tid = _hModbus->trans_id;
    _hModbus->trans_id++;

    return BSP_OK;
}

static void modbus_tcp_request_handle(BSP_ModbusHandleTypedef *_hModbus) /* DEBUG: 野指针排除 */
{
    static enum {
        MB_TCP_IDLE = 0,
        MB_TCP_SEND_REQ,
        MB_TCP_SENDING_INTERVAL
    } mb_tcp_state = MB_TCP_IDLE;
    // static uint8_t timeout_cnt = 0;
    static uint32_t last_tick = 0;
    static uint8_t *pbuf = NULL;
    static uint32_t len = 0;
    struct list_head *pos, *n;
    BSP_StatusTypedef bsp_sta = BSP_BUSY; 

    bsp_assert(_hModbus->Protocol == MB_TCP);

    switch (mb_tcp_state) {
    case MB_TCP_IDLE:
        list_for_each_safe(pos, n, &req_window) {
            if (!((struct req_item*)pos)->tx_flag) {
                ((struct req_item*)pos)->tx_flag = 1;
                len = ((struct req_item*)pos)->tx_len;
                pbuf = (uint8_t *)bsp_malloc(len);
                memcpy(pbuf, ((struct req_item*)pos)->tx_buffer, len);
                // timeout_cnt = 0;
                mb_tcp_state = MB_TCP_SEND_REQ;
                /* DEBUG: 20250520 */
                // list_del(pos);
                // bsp_free(pos);
                /* DEBUG: 20250520 */
                break;
            }
        }
        if (mb_tcp_state != MB_TCP_SEND_REQ) 
            break;
    case MB_TCP_SEND_REQ:
        bsp_sta = _hModbus->mb_ops->SendMessage(pbuf, len);
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK) {
                bsp_free(pbuf);
                len = 0;
                last_tick = bsp_get_systick();
                mb_tcp_state = MB_TCP_SENDING_INTERVAL;
            } else if (bsp_sta == BSP_TIMEOUT) { /* 超时表明发送失败 */ /* FIXME: 根本进不来，没有这个返回值 */
                bsp_assert(0);
                // timeout_cnt++;
                // if (timeout_cnt >= 3) { /* 重试三次发送, 复位当前tcp传输通道 */
                //     list_del(&(p->list));
                //     bsp_free(p);
                //     mb_tcp_state = MB_TCP_IDLE;
                // }
            } else if (bsp_sta == BSP_ERROR) {

            }
        }
        break;
    case MB_TCP_SENDING_INTERVAL:
        if ((bsp_get_systick() - last_tick) > REQ_SENDING_INTERVAL) { /* 报文发送间隔100ms */
            mb_tcp_state = MB_TCP_IDLE;
        }
        break;
    default:
        break;  
    }
}

/**
 * @brief 查询是否有报文返回
 * 
 * @return BSP_StatusTypedef BSP_OK: 当前缓冲区存在接收报文, BSP_BUSY: 当前缓冲区空
 */
BSP_StatusTypedef bsp_modbus_tcp_inquire_response(uint16_t *tid, BSP_ModbusErrorTypeDef *eErrorCode, uint16_t *buffer, uint32_t *len)
{
    struct list_head *pos, *n;
    *tid = 0;
    list_for_each_safe(pos, n, &rsp_window) { // FIXME: 20250618-存在线程安全问题
        *tid = ((struct rsp_item*)pos)->tid;
        *eErrorCode = ((struct rsp_item*)pos)->err_code;
        *len = ((struct rsp_item*)pos)->rx_dlen;
        memcpy(buffer, ((struct rsp_item*)pos)->rx_data, 2*(*len));   /* FIXME: 此处与05命令的返回逻辑不同 */
        list_del(pos);
        bsp_free(pos);
        return BSP_OK;
    }
    return BSP_BUSY;
}

uint32_t lost_msg_cnt = 0;
uint32_t max_msg_len = 0;
uint32_t fifo_len_0 = 0;
uint32_t ms_tick_cnt = 0;
uint32_t ms_tick_period = 0;
uint32_t max_tick_period = 0;
uint32_t ms_last_tick = 0;
static void modbus_tcp_response_handle(BSP_ModbusHandleTypedef *_hModbus)
{
    static uint8_t state = 0;
    /* 中间变量 */
    short stmp = 0;
    /* 临时变量 */
	uint8_t len = 0;
    static uint16_t tid;

    uint8_t tmpbuf8[300];
    uint8_t flag_keep_go = 0;
    static uint32_t msg_len = 0;
    static uint8_t  msg[300];
    static uint32_t recv_cnt = 0;
    static uint16_t reg = 0;
    BSP_ModbusErrorTypeDef eErrorCode;
    uint16_t tmpbuf16[128];
    uint8_t dlen;
	uint32_t i = 0;
	uint32_t j;

    uint8_t idx_flag = 0;
    struct list_head *pos, *n;
    static struct list_head *req_idx = NULL;
    struct list_head *cur_req = NULL;
    struct rsp_item *p_rsp = NULL;

    /* 取出tcp报文 */
    len = _hModbus->mb_ops->msg_fifo_get(tmpbuf8, 300); 
	if (len != 0) {
		fifo_len_0 = len;
        if (fifo_len_0 > max_msg_len) {
            max_msg_len = fifo_len_0;
        }
	}

    if (state & ~((1<<1)-1)) {
        list_for_each_safe(pos, n, &req_window) {
            if (req_idx == pos) {
                cur_req = pos;
                idx_flag++;
                break;
            }
        }

        if (!idx_flag) { /* the req that idx stand for has been removed */ /* reset state */
            req_idx = NULL;
            state = 0;
        }

        bsp_assert((idx_flag && cur_req) || (!idx_flag));    /* the pointer of req must not be NULL */
    }

    while (i < len) {
        reg = (reg << 8) + tmpbuf8[i];
		flag_keep_go = 1;
        while (flag_keep_go) {
            flag_keep_go = 0;
            switch (state) {
            case 0: /* flag_last_char */
                state++;
                break;
            case 1:
                list_for_each_safe(pos, n, &req_window) {   /* 查找匹配的tid */
                    if (reg == ((struct req_item *)pos)->tid) {
                        req_idx = pos;
						cur_req = pos;
                        tid = reg;
                        state++;
                        break;
                    }
                }
                break;
            case 2:
            case 3:
                if (tmpbuf8[i] == 0) {
                    state++;
                    if (state == 4) {
                        list_for_each_safe(pos, n, &req_window) {  /* 移除已丢失的请求 */
                            if (cur_req == pos) {
                                break;
                            } else {
                                lost_msg_cnt++;
                                list_del(pos);
                                bsp_free(pos);
                            }
                        }
                    }
                } else {
                    state = 1;
                    flag_keep_go = 1;
                }
                break;
            case 4:
                state++;
                break;
            case 5:
                state++;
                msg_len = reg;
				recv_cnt = 0;
                break;
            case 6:
                msg[recv_cnt++] = tmpbuf8[i];
                if (recv_cnt == msg_len) {
                    state++;
                }
                break;
            case 7:
            default:
                break;
            }
        }
        /***** 报文解析 *****/
        if (state == 7) {
            /* modbus-tcp报文头检查 */
            if (msg_len < 3) {   /* 9为modbus-tcp协议错误应答报文长度 */ /* 返回的报文长度不符合modbus-tcp定义的最小报文长度（错误应答报文） */
                state = 0;
                goto DROP_MSG;
            }
            /* 检查从机地址 */
            if (msg[0] != ((struct req_item*)cur_req)->tx_buffer[6]) {
                state = 0;
                goto DROP_MSG;
            }
            /* 检查是否为错误应答 */
            if (msg[1] != ((struct req_item*)cur_req)->tx_buffer[7]) {
                if (!(msg[1] & 0x80)) { /* 并非错误应答 */
                    state = 0;
                    goto DROP_MSG;
                }
                if (msg[1] != (((struct req_item*)cur_req)->tx_buffer[7] | 0x80)) {  /* 并非对应的错误应答 */
                    state = 0;
                    goto DROP_MSG;
                }
                _hModbus->ErrLatch = MODBUS_ERRACK;
            } else {
                _hModbus->ErrLatch = MODBUS_SUCESS;
            }

            /* FIXME: 调用旧驱动函数解析返回信息 */
            /* 查找命令索引 */
            for (j = 0; j < _hModbus->CmdLen; j++) {
                if (((struct req_item*)cur_req)->tx_buffer[7] == _hModbus->pcCmd[j]) {
                    _hModbus->CmdIndex = j;
                    break;
                }
            }
            if (j >= _hModbus->CmdLen) {
                state = 0;
                goto DROP_MSG;
            }
            _hModbus->pcTxBuffer = ((struct req_item*)cur_req)->tx_buffer + 6;
            _hModbus->TxCnt = ((struct req_item*)cur_req)->tx_len - 6;
            _hModbus->pcRxBuffer = msg;
            _hModbus->RxCnt = msg_len;
            _hModbus->exe = 1;
            eErrorCode = bsp_MBMasterGetRSP(_hModbus, tmpbuf16, &dlen);
            /* 保存结果至缓冲区中 */
            /* 检查缓冲区是否存在空余 */
            list_for_each_safe(pos, n, &rsp_window) { // FIXME: 20250618-存在线程安全问题
                stmp++;
                if (stmp == RSP_WINDOW_SIZE) {
                    state = 0;
                    goto DROP_MSG;
                }
            }
            
            p_rsp = (struct rsp_item*)bsp_malloc(sizeof(struct rsp_item) + 2*dlen);
            p_rsp->tid = tid;
            p_rsp->err_code = eErrorCode;
            p_rsp->rx_dlen = dlen;
            memcpy(p_rsp->rx_data, tmpbuf16, 2 * p_rsp->rx_dlen);
            list_add_tail(&(p_rsp->list), &rsp_window);

            /* 删除请求元素 */
            list_del(cur_req);
            bsp_free(cur_req);
            state = 0;
        }
    DROP_MSG:
        i++;
        continue;
    }
}

uint8_t bsp_modbus_tcp_req_is_empty(void)
{
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &req_window) {
        return 0;
    }
    return 1;
}

//static void modbus_tcp_check_windwos(void) // FIXME: 20250618-存在线程安全问题
//{
//    struct list_head *pos, *n;
//    list_for_each_safe(pos, n, &rsp_window) {
//        bsp_assert(pos != NULL);
//        bsp_assert((uint32_t)pos != 0xFFFFFFFF);
//    }
//    list_for_each_safe(pos, n, &req_window) {
//        bsp_assert(pos != NULL);
//        bsp_assert((uint32_t)pos != 0xFFFFFFFF);
//    }
//}

void bsp_modbus_tcp_process(BSP_ModbusHandleTypedef *_hModbus) 
{
	// modbus_tcp_check_windwos();
    modbus_tcp_response_handle(_hModbus);
    modbus_tcp_request_handle(_hModbus);
}

void bsp_modbus_tcp_reset_windows(void)
{
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &req_window) {
        list_del(pos);
        bsp_free(pos);
    }
}
