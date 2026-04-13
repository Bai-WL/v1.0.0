/**
 * @file bsp_gd32f303xx_modbus.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-07
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __BSP_GD32F303XX_MODBUS_H__
#define __BSP_GD32F303XX_MODBUS_H__

#include <stdint.h>
#include "bsp_types.h"

typedef enum __BSP_ModbusErrorTypeDef {
    MODBUS_SUCESS = 0,
    /* Slave Error State */
    MODBUS_ERROR01,
    MODBUS_ERROR02,
    MODBUS_ERROR03,
    MODBUS_ERROR04,
    MODBUS_ERROR05,
    MODBUS_ERROR06,
    MODBUS_ERROR07,
    MODBUS_ERROR08,
    MODBUS_ERROR09,
    MODBUS_ERROR0A,
    MODBUS_ERROR0B,
    MODBUS_ERROR0C,
    MODBUS_ERROR0D,
    MODBUS_DEFAULT, // 涵盖所有程序异常而非协议故障的错误
    MODBUS_PASS,
    MODBUS_IDLE,    // 从机模式下无接收到命令，空闲
    /* Master Error State */
    MODBUS_BUSY,
    MODBUS_NACK,    // 从机无应答
    MODBUS_ERRACK,  // 从机错误应答
    MODBUS_ACKERR   // 从机应答错误, 包括从机地址错误、命令码错误、以及CRC错误
} BSP_ModbusErrorTypeDef;

typedef enum __BSP_ModbusModeTypeDef {
    MB_MASTER = 0,
    MB_SLAVE
} BSP_ModbusModeTypeDef;

typedef enum __BSP_ModbusRegRWTypeDef {
    MB_READREG = 0,
    MB_WRITEREG
} BSP_ModbusRegRWTypeDef;

typedef enum __BSP_ModbusProtocolTypeDef {
    MB_RTU = 0,
    MB_ASCII, 
    MB_TCP
} BSP_ModbusProtocolTypeDef;

typedef struct __BSP_ModbusOperationsTypedef {
    void (*HardwareInit) (uint32_t _ulBaud);
    void (*TimerInit) (void);
    void (*MessageISR) (uint8_t ucChar);
    BSP_StatusTypedef (*SendMessage) (const uint8_t *_pcBuf, uint32_t _usLen);
    uint32_t (*GetMessage) (uint8_t *_pcBuf);
    uint32_t (*msg_fifo_get) (uint8_t *_pcBuf, uint32_t len);
    uint32_t (*msg_fifo_len) (void);
    void (*RstTransChan) (void);
} BSP_ModbusOperationsTypedef;

typedef struct __BSP_ModbusHandleTypedef {
    BSP_ModbusModeTypeDef Mode;   // Master or Slave
    BSP_ModbusProtocolTypeDef Protocol; // RTU, ASCII, TCP
    uint8_t SlaveAddr;

    uint8_t exe;        /* 收到应答标志 或 执行应答标志 */
    uint8_t pass;       /* Slave mode 使用，忽略当前报文; Master mode 使用，用于在广播报文时，忽略掉从机的协议性质错误回复。 */
    uint8_t busy;       /* Master mode 使用，当前modbus忙 */
    BSP_ModbusErrorTypeDef ErrLatch;    /* 错误信息 */
    uint32_t TimeoutCnt;
    /* Master & Slave 使用 */
    uint8_t RxCnt;          /* 接收字节计数 */
    uint8_t *pcRxBuffer;    /* 接收缓冲区指针 */
    uint16_t CRCRx;         /* 字节流CRC */
    /* Master & Slave 使用 */
    uint8_t CmdLen;
    uint8_t *pcCmd;
    uint8_t CmdIndex;
    BSP_ModbusErrorTypeDef (**eCmdCallback) (struct __BSP_ModbusHandleTypedef *); /* 命令回调指针数组 */ /* 命令报文解析或回复报文解析 */
    BSP_ModbusErrorTypeDef (**eCmdRSP) (struct __BSP_ModbusHandleTypedef *);
    /* 仅Slave使用 */
    uint8_t BaseAddrCnt;
    uint16_t *psBaseAddr;   /* 参数地址 */
    uint8_t BaseAddrIndex;
    uint16_t *psRegAddrLen; /* 参数地址对应的最大参数个数 */
    uint8_t RegAddrOffset;
    BSP_ModbusErrorTypeDef (**eBaseAddrCallback) (struct __BSP_ModbusHandleTypedef *, BSP_ModbusRegRWTypeDef); /* 寄存器操作回调指针数组 */
    /* Master & Slave 使用 */
    uint16_t DataByteLen;   /* Master 所接收到回复的数据长度 */
    uint8_t *pcDataBuffer;  /* 接收或发送的数据缓冲区指针 */
    uint16_t TxCnt;         /* Master命令报文发送长度 */
    uint8_t *pcTxBuffer;    /* 地址空间常量指针，用于给DMA发送提供数据空间，避免由于栈空间释放导致DMA读写空指针地址 */
    /* Modbus Tcp 报文头解析使用 */
    uint8_t MsgHeadcnt;
    uint16_t trans_id;
    uint16_t pid;
    uint16_t msg_len;
    /* Modbus相关操作集 */
    BSP_ModbusOperationsTypedef *mb_ops;
} BSP_ModbusHandleTypedef;

typedef BSP_ModbusErrorTypeDef (*eCmdCallbackTypeDef) (BSP_ModbusHandleTypedef *);
typedef BSP_ModbusErrorTypeDef (*eBaseAddrCallbackTypeDef) (BSP_ModbusHandleTypedef *, BSP_ModbusRegRWTypeDef);
typedef BSP_ModbusErrorTypeDef (*eCmdRSPTypeDef) (BSP_ModbusHandleTypedef *);

void bsp_ModbusInit(BSP_ModbusHandleTypedef *_hModbus, BSP_ModbusModeTypeDef _eMode, BSP_ModbusProtocolTypeDef _eProtocol, uint32_t _ulBaud, BSP_ModbusOperationsTypedef *_eMBOps);

void bsp_ModbusUartTimIRQCallback(BSP_ModbusHandleTypedef *_hModbus);
void bsp_ModbusUartTxIRQCallback(BSP_ModbusHandleTypedef *_hModbus);
void bsp_ModbusUartRxIRQCallback(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);

BSP_ModbusErrorTypeDef bsp_MBMasterGetRSP(BSP_ModbusHandleTypedef *_hModbus, uint16_t *_pcRxBuf, uint8_t *_ucRxLen); // TODO: 暂留，待新代码完全验证稳定性后移除
BSP_StatusTypedef bsp_MBMasterTransmitCommand(BSP_ModbusHandleTypedef *_hModbus,
                                                uint8_t _ucSlaveAddr,
                                                uint8_t _ucCmd, 
                                                uint16_t _ucRegAddr, 
                                                uint16_t *_psData, 
                                                uint8_t _ucDataLen, 
                                                BSP_ModbusErrorTypeDef *eErrorCode, 
                                                uint16_t *_pcRxBuf, 
                                                uint8_t *_ucRxLen);
// BSP_StatusTypedef bsp_MBMasterTransmitCommand(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucSlaveAddr, uint8_t _ucCmd, uint16_t _ucRegAddr, uint16_t *_psData, uint8_t _ucDataLen);
BSP_ModbusErrorTypeDef bsp_MBSlaveFunctionExecute(BSP_ModbusHandleTypedef *_hModbus);

uint16_t bsp_MBCRC16(uint8_t *data, uint32_t len);

#endif /* __BSP_GD32F303XX_MODBUS_H__ */
