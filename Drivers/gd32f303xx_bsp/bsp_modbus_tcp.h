/**
 * @file bsp_modbus_tcp.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-05-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __BSP_MODBUS_TCP_H__
#define __BSP_MODBUS_TCP_H__
#include "bsp_types.h"
#include "bsp_gd32f303xx_modbus.h"

#define REQ_WINDOW_SIZE 4
#define RSP_WINDOW_SIZE 4
#define REQ_SENDING_INTERVAL 25    /* µ¥Î»: ms */

BSP_StatusTypedef bsp_modbus_tcp_submit_request(BSP_ModbusHandleTypedef *_hModbus,
                                                uint8_t link_id,
                                                uint16_t *tid,
                                                uint8_t _ucSlaveAddr,
                                                uint8_t _ucCmd, 
                                                uint16_t _ucRegAddr, 
                                                uint16_t *_psData, 
                                                uint8_t _ucDataLen);
BSP_StatusTypedef bsp_modbus_tcp_inquire_response(uint16_t *tid, BSP_ModbusErrorTypeDef *eErrorCode, uint16_t *buffer, uint32_t *len);
void bsp_modbus_tcp_process(BSP_ModbusHandleTypedef *_hModbus);
void bsp_modbus_tcp_window_init(void);
void bsp_modbus_tcp_reset_windows(void);
uint8_t bsp_modbus_tcp_req_is_empty(void);
#endif /* __BSP_MODBUS_TCP_H__ */
