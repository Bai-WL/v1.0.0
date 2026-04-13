/**
 * @file bsp_gd32f303xx_modbus.c
 * @author your name (you@domain.com)
 * @brief Modbus软件层
 * @version 0.3
 * @date 2025-04-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stddef.h>
#include <stdio.h>
#include "gd32f30x_gpio.h"
#include "gd32f30x_timer.h"
#include "bsp_types.h"
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_gd32f303xx_mbuser.h"
#include "bsp_gd32f303xx_mbboard.h"
#include "bsp_gd32f303xx_systick.h"

/* TODO: 对大小端转换相关的功能块使用内核自带的函数__REV16实现 */
/* FIXME: 缺少对广播地址的响应功能 */
/* FIXME: 需要对出现故障的无法通讯的从机地址打上标记 */

/* 调试用临时头文件 */
#include "bsp_gd32f303xx_jlx192128g.h"

/* private prototype */
static BSP_StatusTypedef vMBSoftInit(BSP_ModbusHandleTypedef *_hModbus, BSP_ModbusModeTypeDef _eMode, BSP_ModbusProtocolTypeDef _eProtocol, BSP_ModbusOperationsTypedef *_eMBOps);

static void vMBSlaveUartTxISR(BSP_ModbusHandleTypedef *_hModbus);
static void vMBSlaveTimISR(BSP_ModbusHandleTypedef *_hModbus);
static void vMBSlaveUartRxISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);
static BSP_ModbusErrorTypeDef eMBSlaveNormalRSP(BSP_ModbusHandleTypedef *_hModbus);
static void vMBSlaveExceptionRSP(BSP_ModbusHandleTypedef *_hModbus);
static void vResetSlaveRxState(BSP_ModbusHandleTypedef *_hModbus);
static BSP_StatusTypedef eCheckRegLen(BSP_ModbusHandleTypedef *_hModbus, uint16_t _usCnt);
static BSP_StatusTypedef eCheckRegAddr(BSP_ModbusHandleTypedef *_hModbus, uint16_t _usAddr);
static BSP_StatusTypedef eCheckCmdCode(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);
static BSP_ModbusErrorTypeDef eMBSlaveReceive(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);

static void vMBTCPMasterUartRXISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);

static void vMBMasterUartTxISR(BSP_ModbusHandleTypedef *_hModbus);
static void vMBMasterUartRxISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);
static void vMBMasterTimISR(BSP_ModbusHandleTypedef *_hModbus);
static void vResetMasterState(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eMBMasterExceptionRSP(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eMBMasterNormalRSP(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eMBMasterReceive(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar);

static void vMakeCRC16Table(void);
static uint16_t usCRC16Stream(uint16_t _usLastCRC, uint8_t data);
static BSP_ModbusErrorTypeDef vCmdFun01(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef vCmdFun03(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef vCmdFun05(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef vCmdFun06(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef vCmdFun10(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eCmdRSP01(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eCmdRSP03(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eCmdRSP05(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eCmdRSP06(BSP_ModbusHandleTypedef *_hModbus);
static BSP_ModbusErrorTypeDef eCmdRSP10(BSP_ModbusHandleTypedef *_hModbus);

/* global config variable */
uint8_t g_pCmdSet[MODBUS_FUNCTIONS_COUNTS] = {
    0x01,
    0x03,
    0x05, 
    0x06, 
    0x10
};

eCmdCallbackTypeDef g_pCmdFunSet[MODBUS_FUNCTIONS_COUNTS] = {
    vCmdFun01, 
    vCmdFun03, 
    vCmdFun05, 
    vCmdFun06, 
    vCmdFun10
};

static eCmdRSPTypeDef g_pCmdRSPSet[MODBUS_FUNCTIONS_COUNTS] = {
    eCmdRSP01,
    eCmdRSP03,
    eCmdRSP05,
    eCmdRSP06, 
    eCmdRSP10
};

/* user config variable extern */
extern uint16_t g_pBaseAddrSet[MODBUS_BASEADDR_COUNTS];
extern uint16_t g_pRegAddrLen[MODBUS_BASEADDR_COUNTS];
extern eBaseAddrCallbackTypeDef g_pBaseAddrFunSet[MODBUS_BASEADDR_COUNTS];

/* global variable */
static uint16_t CRC16LUT[256] = {0};
static uint8_t g_pFrameRxBuffer[256] = {0};   /* TODO: 需优化为链表形式 */
static uint8_t g_pFrameTxBuffer[256] = {0};   /* DMA发送使用数组 */

/**
 * @brief modbus初始化函数
 * 
 * @param _hModbus modbus全局结构体变量
 * @param _eMode MB_MASTER 或 MB_SLAVE
 * @param _ulBaud 波特率设置
 */
void bsp_ModbusInit(BSP_ModbusHandleTypedef *_hModbus, BSP_ModbusModeTypeDef _eMode, BSP_ModbusProtocolTypeDef _eProtocol, uint32_t _ulBaud, BSP_ModbusOperationsTypedef *_eMBOps)
{    
	vMBSoftInit(_hModbus, _eMode, _eProtocol, _eMBOps);
    _hModbus->mb_ops->TimerInit();
    _hModbus->mb_ops->HardwareInit(_ulBaud);
}

static BSP_StatusTypedef vMBSoftInit(BSP_ModbusHandleTypedef *_hModbus, BSP_ModbusModeTypeDef _eMode, BSP_ModbusProtocolTypeDef _eProtocol, BSP_ModbusOperationsTypedef *_eMBOps)
{
    if ((_eMode != MB_MASTER) && (_eMode != MB_SLAVE)) {
        return BSP_ERROR;
    }

    _hModbus->Mode  = _eMode;     // Master or Slave
    if (_hModbus->Mode == MB_SLAVE) {
        _hModbus->SlaveAddr = MODBUS_SLAVE_ADDR;
    } else {
        _hModbus->SlaveAddr = 0;
    }

    _hModbus->Protocol = _eProtocol;

    _hModbus->exe   = 0;      /* 功能码接收完毕后置1，表明函数可执行 */ /* 仅在帧结束的定时器中断中有权限置1 */
    _hModbus->pass  = 0;     /* 忽略当前指令 */ /* 有权限控制该变量的对象为 1. 从地址不匹配； 2. 等待函数执行时接收到字节流 */
    _hModbus->busy  = 0;
    _hModbus->ErrLatch = MODBUS_DEFAULT; /* 当ExecuteLatch初始状态时，默认置位，接收完毕才更改为SUCCESS */
    _hModbus->TimeoutCnt = 0;

    _hModbus->RxCnt = 0;
    if (_hModbus->Protocol == MB_TCP) {
        _hModbus->pcRxBuffer = g_pFrameRxBuffer + 6;    
    } else {
        _hModbus->pcRxBuffer = g_pFrameRxBuffer;
    }
    _hModbus->CRCRx = 0xFFFF;

    _hModbus->CmdLen = MODBUS_FUNCTIONS_COUNTS;
    _hModbus->pcCmd = g_pCmdSet;
    _hModbus->CmdIndex = 0;
    _hModbus->eCmdCallback = g_pCmdFunSet; /* 命令回调指针数组 */
    _hModbus->eCmdRSP = g_pCmdRSPSet;

    _hModbus->BaseAddrCnt = MODBUS_BASEADDR_COUNTS;
    _hModbus->psBaseAddr = g_pBaseAddrSet;   /* 参数地址 */
    _hModbus->BaseAddrIndex = 0;
    _hModbus->psRegAddrLen = g_pRegAddrLen; /* 参数地址对应的最大参数个数 */
    _hModbus->RegAddrOffset = 0;
    _hModbus->eBaseAddrCallback = g_pBaseAddrFunSet; /* 寄存器操作回调指针数组 */

    _hModbus->DataByteLen = 0;
    _hModbus->pcDataBuffer = NULL;

    _hModbus->TxCnt = 0;
    if (_hModbus->Protocol == MB_TCP) {
        _hModbus->pcTxBuffer = g_pFrameTxBuffer + 6;    /* 对TCP发送报文进行报文头位置偏移 */
    } else {
        _hModbus->pcTxBuffer = g_pFrameTxBuffer;
    }

    _hModbus->MsgHeadcnt = 0;
    _hModbus->trans_id = 0;
    _hModbus->pid = 0;  /* modbus tcp 协议描述符 */ 
    _hModbus->msg_len = 0;

    _hModbus->mb_ops = _eMBOps;

    vMakeCRC16Table();

    return BSP_OK;
}

void bsp_ModbusUartTxIRQCallback(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        vMBMasterUartTxISR(_hModbus);
    } else if (_hModbus->Mode == MB_SLAVE) {
        vMBSlaveUartTxISR(_hModbus);
    }
}

void bsp_ModbusUartRxIRQCallback(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{
    if (_hModbus->Mode == MB_MASTER) {
		if (_hModbus->Protocol == MB_TCP) {
			vMBTCPMasterUartRXISR(_hModbus, _ucChar);
		} else {
			vMBMasterUartRxISR(_hModbus, _ucChar);
		}
    } else if (_hModbus->Mode == MB_SLAVE) {
        vMBSlaveUartRxISR(_hModbus, _ucChar);
    }
}

void bsp_ModbusUartTimIRQCallback(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        vMBMasterTimISR(_hModbus);
    } else if (_hModbus->Mode == MB_SLAVE) {
        vMBSlaveTimISR(_hModbus);
    }
}

/* ===========================================TCP主机函数=========================================== */
/**
 * @brief modbus tcp不以时间间隔标记帧头帧尾，而是在报文头中声明帧长度
 *        modbus tcp不需要进行CRC校验，但需要对报文头进行核对
 *        追加了TCP协议下的接收中断处理函数，在其中进行了对TCP协议下的报文头部分的处理，并直接在该函数中判断帧结束标志并置位。TODO: 后续可能需要根据TCP协议的帧特性，实现队列形式的命令处理。
 * @param _hModbus 
 */
static void vMBTCPMasterUartRXISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{
    static uint16_t usRxReg = 0;
    BSP_ModbusErrorTypeDef eErrorCode = MODBUS_DEFAULT;
    MBTIM_DISABLE();
    MBTIM_LOAD_RST();
    _hModbus->TimeoutCnt = 0;
    if (_hModbus->ErrLatch != MODBUS_ACKERR) {  /* 若接收帧格式错误，忽略后续接收 */
        if (_hModbus->exe) {  /* 若已成功接收到一帧数据但未进行处理，直接返回 */
            return ;
        }
        if (_hModbus->MsgHeadcnt < 6) {
            g_pFrameRxBuffer[_hModbus->MsgHeadcnt] = _ucChar;
            switch (_hModbus->MsgHeadcnt) {
            case 0: usRxReg = (usRxReg<<8) + _ucChar; break;
            case 1: usRxReg = (usRxReg<<8) + _ucChar;
                if ((_hModbus->trans_id - 1) != usRxReg) {
                    _hModbus->ErrLatch = MODBUS_ACKERR;
                }
                break;
            case 2: usRxReg = (usRxReg<<8) + _ucChar; break;
            case 3: usRxReg = (usRxReg<<8) + _ucChar;
                if (_hModbus->pid != usRxReg) {
                    _hModbus->ErrLatch = MODBUS_ACKERR;
                }
                break;
            case 4: usRxReg = (usRxReg<<8) + _ucChar; break;
            case 5: usRxReg = (usRxReg<<8) + _ucChar;
                _hModbus->msg_len = usRxReg;
                break;
            default:
                break;
            }
            _hModbus->MsgHeadcnt++;
            MBTIM_ENABLE();
        } else {    /* 帧结束判断 */
            eErrorCode = eMBMasterReceive(_hModbus, _ucChar);
            if (_hModbus->ErrLatch == MODBUS_DEFAULT && eErrorCode != MODBUS_DEFAULT) {
                _hModbus->ErrLatch = eErrorCode;
            } 
        }
        if (_hModbus->RxCnt < _hModbus->msg_len) {
            MBTIM_ENABLE();
        } else {    /* 完成一帧接收 */
            if (_hModbus->ErrLatch == MODBUS_DEFAULT) {
                _hModbus->ErrLatch = MODBUS_SUCESS;
            }
            MBTIM_LOAD_RST();
            MBTIM_DISABLE();
            _hModbus->exe = 1;
        }
    } else {    /* 当modbus当前帧传输错误，直接执行报文处理 */
        MBTIM_ENABLE(); /* 超时销帧，等待通道清空 */
    }
}
/* ===========================================TCP主机函数=========================================== */

/* ===========================================RTU主机函数=========================================== */
static void vMBMasterUartTxISR(BSP_ModbusHandleTypedef *_hModbus)
{
    MBTIM_DISABLE();    /* 关闭定时器 */
    MBTIM_LOAD_RST();   /* 定时器清零 */
    MBTIM_ENABLE();     /* 开启定时器 */
}

uint32_t mb_tx_tick = 0;
uint32_t mb_tick_cnt = 0;

/**
 * @brief Tim中断时刻，是一次modbus通信的结尾
 *      （TODO: 但在该定时器中断服务中，并没有做到完善，缺陷在于只能供一个句柄使用，后续需要进行进一步的多线程兼容）
 * @param _hModbus 
 */
static void vMBMasterTimISR(BSP_ModbusHandleTypedef *_hModbus)
{
    if ((_hModbus->Protocol == MB_RTU && _hModbus->RxCnt)   /* RTU帧结束标志 */ 
        || (_hModbus->Protocol == MB_TCP && _hModbus->ErrLatch == MODBUS_ACKERR)) {  /* TCP帧格式错误，等待通道空闲 */
        MBTIM_DISABLE();    /* 关闭定时器 */
        MBTIM_LOAD_RST();   /* 定时器清零 */
        
        if (_hModbus->pass) {     /* 忽略当前接收完毕的指令 */
            _hModbus->pass = 0;   /* 忽略并重置pass锁存器 */
            vResetMasterState(_hModbus);
        } else {
            if (_hModbus->CRCRx) {  // CRC的错误优先级最高，报错信息优先
                _hModbus->ErrLatch = MODBUS_ACKERR;
            } else if (_hModbus->ErrLatch == MODBUS_DEFAULT) {
                _hModbus->ErrLatch = MODBUS_SUCESS;
            }
            _hModbus->exe = 1;  /* 执行回应 */
        }
    } else if (_hModbus->Protocol == MB_TCP || !_hModbus->RxCnt) {  /* 丢帧超时计数 */
        /* modbus超时计数，情况1： 在RTU协议下，在规定的超时时间内，若从机未进行任何回复，则超时；情况2：在TCP协议下，在规定的超时时间内，若未完成报文头声明数目字节的接收，则超时 */ 
        _hModbus->TimeoutCnt++;
        if (_hModbus->TimeoutCnt >= MODBUS_RSP_TIMEOUT/10) {
			mb_tick_cnt = bsp_get_systick() - mb_tx_tick;
            MBTIM_DISABLE();    /* 关闭定时器 */
            MBTIM_LOAD_RST();   /* 定时器清零 */

            _hModbus->ErrLatch = MODBUS_NACK;
            _hModbus->exe = 1;  /* 执行回应 */
        }
    }
}

static void vMBMasterUartRxISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{
    BSP_ModbusErrorTypeDef eErrorCode = MODBUS_DEFAULT;
    MBTIM_DISABLE();
    MBTIM_LOAD_RST();

    if (_hModbus->ErrLatch != MODBUS_ACKERR) {  /* 若接收帧格式错误，忽略后续接收 */
        _hModbus->CRCRx = usCRC16Stream(_hModbus->CRCRx, _ucChar);  /* CRC字节流校验 */

        eErrorCode = eMBMasterReceive(_hModbus, _ucChar);
        if (eErrorCode != MODBUS_DEFAULT) { /* TODO: 为什么此处不判断_hModbus->ErrLatch是否存在错误应答 */
            _hModbus->ErrLatch = eErrorCode;
        } 
    }

    MBTIM_ENABLE();
}

static BSP_ModbusErrorTypeDef eMBMasterReceive(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{
/* 存储接收状态机序列位置 */
    _hModbus->pcRxBuffer[_hModbus->RxCnt] = _ucChar;   /* 接收字节流 */

    switch (_hModbus->RxCnt++) {
        case 0: {
            if (_hModbus->SlaveAddr != _ucChar) {
                return MODBUS_ACKERR;
            }
            break;
        }
        case 1: {
            /* check validation of cmdcode */
            if (_ucChar & 0x80) {
                if (_hModbus->pcCmd[_hModbus->CmdIndex] != (_ucChar & 0x7F)) {
                    return MODBUS_ACKERR;
                }
                return MODBUS_ERRACK;
            } else if (_hModbus->pcCmd[_hModbus->CmdIndex] != _ucChar) {
                return MODBUS_ACKERR;
            }
            /* save Cmd code */
        } break;
        default : break;
    }

    return MODBUS_DEFAULT;
}

/**
 * @brief 主机命令发送函数
 * 
 * @param _hModbus      modbus句柄
 * @param _ucSlaveAddr  从机地址 
 * @param _ucCmd        0x03 0x06 0x10 0x01 0x05
 * @param _ucRegAddr    指定要读写的从机寄存器地址
 * @param _psData       发送数据时，需传入的u16类型数据数组指针
 * @param _ucDataLen    当命令为写命令时，该参数为发送数据的长度；当命令为读时，该参数为所读数据寄存器的长度
 * @param eErrorCode    判定命令是否被正确执行及被应答
 * @param _pcRxBuf      接收数组指针
 * @param _ucRxLen      接收数组的长度，若长度不为零，则对应命令为读命令；若长度为零，则对应命令为写命令
 * @return BSP_StatusTypedef BSP_BUSY: 通信中; BSP_OK: 通信执行完毕; BSP_ERROR: 通信参数错误
 */
BSP_StatusTypedef bsp_MBMasterTransmitCommand(BSP_ModbusHandleTypedef *_hModbus,
                                                uint8_t _ucSlaveAddr,
                                                uint8_t _ucCmd, 
                                                uint16_t _ucRegAddr, 
                                                uint16_t *_psData, 
                                                uint8_t _ucDataLen, 
                                                BSP_ModbusErrorTypeDef *eErrorCode, 
                                                uint16_t *_pcRxBuf, 
                                                uint8_t *_ucRxLen)
{
    static enum {
        MB_IDLE = 0,
        MB_SEND_CMD,
        MB_WAIT_RSP
    } mb_state = MB_IDLE;
    BSP_StatusTypedef bsp_sta = BSP_BUSY; 
    uint16_t usCRC = 0;

    switch (mb_state) {
    case MB_IDLE:
        if (_hModbus->busy) 
            return BSP_BUSY;

        if (eCheckCmdCode(_hModbus, _ucCmd) != BSP_OK) 
            return BSP_ERROR;

        _hModbus->SlaveAddr = _ucSlaveAddr;

        if (_ucDataLen) {
            _hModbus->DataByteLen = _ucDataLen*2;
            _hModbus->pcDataBuffer = (uint8_t *) _psData;
        }
        if (_hModbus->Protocol == MB_TCP) { /* 添加MODBUS_TCP专属报文头 */
            g_pFrameTxBuffer[0] = (_hModbus->trans_id>>8) & 0xFF;
            g_pFrameTxBuffer[1] = _hModbus->trans_id & 0xFF;
            g_pFrameTxBuffer[2] = (_hModbus->pid>>8) & 0xFF;
            g_pFrameTxBuffer[3] = _hModbus->pid & 0xFF;
            _hModbus->trans_id++;
        }
        _hModbus->pcTxBuffer[0] = (_ucRegAddr>>8) & 0xFF;
        _hModbus->pcTxBuffer[1] = _ucRegAddr & 0xFF;

        if (_hModbus->eCmdCallback[_hModbus->CmdIndex] != NULL) {
            if (_hModbus->eCmdCallback[_hModbus->CmdIndex](_hModbus)) {
                return BSP_ERROR;
            }
        } else {
            return BSP_ERROR;
        }

        if (_hModbus->Protocol == MB_TCP) { /* 对PDU数据段长度进行计算，并添加至TCP报文头中 */
            _hModbus->msg_len = _hModbus->TxCnt;
            g_pFrameTxBuffer[4] = (_hModbus->msg_len>>8) &0xFF;
            g_pFrameTxBuffer[5] = _hModbus->msg_len & 0xFF;
        } else if (_hModbus->Protocol == MB_RTU) {  /* 向RTU报文中添加CRC校验码 */
            usCRC = bsp_MBCRC16(_hModbus->pcTxBuffer, _hModbus->TxCnt);
            _hModbus->pcTxBuffer[_hModbus->TxCnt++] = usCRC & 0xFF; /* CRC先发送低位，再发送高位 */
            _hModbus->pcTxBuffer[_hModbus->TxCnt++] = (usCRC>>8) & 0xFF; 
        } else if (_hModbus->Protocol == MB_RTU) {
            __NOP();
        }

        mb_state = MB_SEND_CMD;
    case MB_SEND_CMD:
		_hModbus->busy = 1;
        bsp_sta = _hModbus->mb_ops->SendMessage(g_pFrameTxBuffer, _hModbus->TxCnt + 6 * (_hModbus->Protocol == MB_TCP));
        if (bsp_sta != BSP_BUSY) {
            if (bsp_sta == BSP_OK || bsp_sta == BSP_TIMEOUT) {
				
				mb_tx_tick = bsp_get_systick();
				
                MBTIM_DISABLE();    /* 关闭定时器 */
                MBTIM_LOAD_RST();   /* 定时器清零 */
                MBTIM_ENABLE();     /* 开启定时器 */
                mb_state = MB_WAIT_RSP;
                /* 不对发送超时进行处理，使modbus处超时配置为唯一超时标准，避免了超时时间不唯一的问题 */
//            } else if (bsp_sta == BSP_TIMEOUT) {
//                mb_state = MB_IDLE;
//				*eErrorCode = MODBUS_NACK;
//				return BSP_TIMEOUT;
            } else if (bsp_sta == BSP_ERROR) {
                _hModbus->busy = 0;
                mb_state = MB_IDLE;
				return BSP_ERROR;
            }
        }     
        break;
    case MB_WAIT_RSP:
        if (_hModbus->exe) {
            *eErrorCode = MODBUS_SUCESS;
            *_ucRxLen = 0;
            if (!_hModbus->ErrLatch) {
                _hModbus->pcDataBuffer = (uint8_t *) _pcRxBuf;
                _hModbus->ErrLatch = eMBMasterNormalRSP(_hModbus);
            }

            if (_hModbus->ErrLatch) {
                *eErrorCode = eMBMasterExceptionRSP(_hModbus);
            } else {
                *_ucRxLen = _hModbus->DataByteLen/2;
            }
			
            vResetMasterState(_hModbus);
            mb_state = MB_IDLE;

			return BSP_OK;
        }
        break;
    default:
        break;
    }

    return BSP_BUSY;
    
    // if (_hModbus->busy) {
    //     return BSP_BUSY;
    // }

    // if (eCheckCmdCode(_hModbus, _ucCmd) != BSP_OK) {
    //     return BSP_ERROR;
    // }

    // _hModbus->SlaveAddr = _ucSlaveAddr;

    // if (_ucDataLen) {
    //     _hModbus->DataByteLen = _ucDataLen*2;
    //     _hModbus->pcDataBuffer = (uint8_t *) _psData;
    // }
    // if (_hModbus->Protocol == MB_TCP) { /* 添加MODBUS_TCP专属报文头 */
    //     g_pFrameTxBuffer[0] = (_hModbus->trans_id>>8) & 0xFF;
    //     g_pFrameTxBuffer[1] = _hModbus->trans_id & 0xFF;
    //     g_pFrameTxBuffer[2] = (_hModbus->pid>>8) & 0xFF;
    //     g_pFrameTxBuffer[3] = _hModbus->pid & 0xFF;
    //     _hModbus->trans_id++;
    // }
    // _hModbus->pcTxBuffer[0] = (_ucRegAddr>>8) & 0xFF;
    // _hModbus->pcTxBuffer[1] = _ucRegAddr & 0xFF;

    // if (_hModbus->eCmdCallback[_hModbus->CmdIndex] != NULL) {
    //     if (_hModbus->eCmdCallback[_hModbus->CmdIndex](_hModbus)) {
    //         return BSP_ERROR;
    //     }
    // } else {
    //     return BSP_ERROR;
    // }

    // if (_hModbus->Protocol == MB_TCP) { /* 对PDU数据段长度进行计算，并添加至TCP报文头中 */
    //     _hModbus->msg_len = _hModbus->TxCnt;
    //     g_pFrameTxBuffer[4] = (_hModbus->msg_len>>8) &0xFF;
    //     g_pFrameTxBuffer[5] = _hModbus->msg_len & 0xFF;
    //     _hModbus->mb_ops->SendMessage(g_pFrameTxBuffer, _hModbus->TxCnt + 6);
    // } else if (_hModbus->Protocol == MB_RTU) {  /* 向RTU报文中添加CRC校验码 */
    //     usCRC = bsp_MBCRC16(_hModbus->pcTxBuffer, _hModbus->TxCnt);
    //     _hModbus->pcTxBuffer[_hModbus->TxCnt++] = (usCRC>>8) & 0xFF;
    //     _hModbus->pcTxBuffer[_hModbus->TxCnt++] = usCRC & 0xFF; 
    //     _hModbus->mb_ops->SendMessage(_hModbus->pcTxBuffer, _hModbus->TxCnt);
    // } else if (_hModbus->Protocol == MB_RTU) {
    //     __NOP();
    // }

    // _hModbus->busy = 1;
    
    // MBTIM_DISABLE();    /* 关闭定时器 */
    // MBTIM_LOAD_RST();   /* 定时器清零 */
    // MBTIM_ENABLE();     /* 开启定时器 */
    
    // return BSP_OK;
}

/**
 * @brief 主机命令应答接收函数
 * 
 * @param _hModbus 
 * @param _pcRxBuf 接收数组指针
 * @param _ucRxLen 接收数组的长度，若长度不为零，则对应命令为读命令；若长度为零，则对应命令为写命令
 * @return BSP_ModbusErrorTypeDef 判定命令是否被正确执行及被应答
 */
BSP_ModbusErrorTypeDef bsp_MBMasterGetRSP(BSP_ModbusHandleTypedef *_hModbus, uint16_t *_pcRxBuf, uint8_t *_ucRxLen)
{
    BSP_ModbusErrorTypeDef eErrorCode = MODBUS_SUCESS;
	*_ucRxLen = 0;
	
    if (_hModbus->exe == 0) {
        return MODBUS_BUSY;
    }
	
    if (!_hModbus->ErrLatch) {
        _hModbus->pcDataBuffer = (uint8_t *) _pcRxBuf;
        _hModbus->ErrLatch = eMBMasterNormalRSP(_hModbus);
    }

    if (_hModbus->ErrLatch) {
        eErrorCode = eMBMasterExceptionRSP(_hModbus);
    } else {
        *_ucRxLen = _hModbus->DataByteLen/2;
    }

    vResetMasterState(_hModbus);

    return eErrorCode;
}

static BSP_ModbusErrorTypeDef eMBMasterNormalRSP(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->eCmdRSP[_hModbus->CmdIndex] != NULL) {
        return (_hModbus->eCmdRSP[_hModbus->CmdIndex](_hModbus));
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef eMBMasterExceptionRSP(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->ErrLatch == MODBUS_ERRACK) {
        if ((_hModbus->Protocol == MB_TCP && _hModbus->RxCnt != 3) || (_hModbus->Protocol == MB_RTU && _hModbus->RxCnt != 5)) {
            return MODBUS_ACKERR;
        } else {
            return ((BSP_ModbusErrorTypeDef) _hModbus->pcRxBuffer[2]);  /* 具体故障信息 */
        }
    } else {
        return _hModbus->ErrLatch;
    }
}

/**
 * @brief modbus句柄复位
 * 
 * @param _hModbus 
 */
static void vResetMasterState(BSP_ModbusHandleTypedef *_hModbus)
{
    _hModbus->busy = 0;
    _hModbus->exe = 0;
    _hModbus->ErrLatch = MODBUS_DEFAULT;
    _hModbus->CRCRx = 0xFFFF;
    _hModbus->RxCnt = 0;
    _hModbus->TimeoutCnt = 0;

    _hModbus->MsgHeadcnt = 0;

    _hModbus->DataByteLen = 0;

    _hModbus->mb_ops->RstTransChan();
}

/* ===========================================主机函数=========================================== */

/* ===========================================中断回调=========================================== */
static void vMBSlaveUartRxISR(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)  /* Modbus从机接收中断处理函数 */
{
    MBTIM_DISABLE();
    MBTIM_LOAD_RST();

    if (_hModbus->exe) {
        _hModbus->pass = 1;
    }

    if (!_hModbus->pass) {
        _hModbus->CRCRx = usCRC16Stream(_hModbus->CRCRx, _ucChar);  /* CRC字节流校验 */
        if (_hModbus->ErrLatch == MODBUS_DEFAULT) {
            _hModbus->ErrLatch = eMBSlaveReceive(_hModbus, _ucChar);
            if (_hModbus->ErrLatch == MODBUS_PASS) { /* 仅当地址不匹配时 */
                _hModbus->pass = 1;
            }
        }
    }

    MBTIM_ENABLE();
}

static void vMBSlaveUartTxISR(BSP_ModbusHandleTypedef *_hModbus)
{
    
}

static void vMBSlaveTimISR(BSP_ModbusHandleTypedef *_hModbus)     /* 完成一帧指令接收的中断 */
{
    MBTIM_DISABLE();    /* 关闭定时器 */
    MBTIM_LOAD_RST();   /* 定时器清零 */
	
    if (_hModbus->pass) {     /* 忽略当前接收完毕的指令 */
        _hModbus->pass = 0;   /* 忽略并重置pass锁存器 */
        vResetSlaveRxState(_hModbus);
    } else {
        if (_hModbus->CRCRx) {  // CRC的错误优先级最高，报错信息优先
            _hModbus->ErrLatch = MODBUS_ERROR08;
        } else if (_hModbus->ErrLatch == MODBUS_DEFAULT) {
            _hModbus->ErrLatch = MODBUS_SUCESS;
        }
        _hModbus->exe = 1;  /* 执行回应 */
    }
}
/* ===========================================中断回调=========================================== */

/* ===========================================主循环执行=========================================== */
BSP_ModbusErrorTypeDef bsp_MBSlaveFunctionExecute(BSP_ModbusHandleTypedef *_hModbus)
{
    BSP_ModbusErrorTypeDef eErrorCode = MODBUS_SUCESS;

    if (_hModbus->exe == 0) {
        return MODBUS_IDLE;
    }

    if (!_hModbus->ErrLatch) {
        _hModbus->ErrLatch = eMBSlaveNormalRSP(_hModbus); // 在执行功能时检查是否出错
    }

    if (_hModbus->ErrLatch && (_hModbus->ErrLatch != MODBUS_DEFAULT)) {
        eErrorCode = _hModbus->ErrLatch;
        vMBSlaveExceptionRSP(_hModbus);
    }
    
    vResetSlaveRxState(_hModbus);   /* 完成应答之后，复位接收状态 */

    return eErrorCode;
}

static BSP_ModbusErrorTypeDef eMBSlaveNormalRSP(BSP_ModbusHandleTypedef *_hModbus)
{
    BSP_ModbusErrorTypeDef eErrorCode = MODBUS_SUCESS;

    /* 命令执行与报文回复 */
    // 执行命令，并检查命令执行是否出错
    if (_hModbus->eCmdCallback[_hModbus->CmdIndex] != NULL) {
        eErrorCode = _hModbus->eCmdCallback[_hModbus->CmdIndex](_hModbus);
    } else {
        return MODBUS_ERROR0A;
    }
    if (eErrorCode) return eErrorCode;

    /* 执行应答 */
    if (_hModbus->eCmdRSP[_hModbus->CmdIndex] != NULL) {    
        _hModbus->eCmdRSP[_hModbus->CmdIndex](_hModbus);
    } else {
        return MODBUS_ERROR0A;
    }

    /* 清除执行标志，允许接收新的modbus指令 */
    vResetSlaveRxState(_hModbus);

    return MODBUS_SUCESS;
}

static void vMBSlaveExceptionRSP(BSP_ModbusHandleTypedef *_hModbus) 
{
	#define ARRAYSIZE(a) (sizeof(a) / sizeof(*a))
		
    char ucTxBuf[64] = {0};
	uint8_t ucDMATxBuf[5] = {0};
    uint16_t usCRC = 0;

    ucDMATxBuf[0] = MODBUS_SLAVE_ADDR;
    ucDMATxBuf[1] = 0x80 + _hModbus->pcRxBuffer[1];
    ucDMATxBuf[2] = _hModbus->ErrLatch;

    usCRC = bsp_MBCRC16(ucDMATxBuf, 3);
    ucDMATxBuf[3] = usCRC & 0xFF;
    ucDMATxBuf[4] = (usCRC >> 8) & 0xFF;

    // sprintf(ucDMATxBuf, "Modbus Rx Error: ERROR0%X\r\n", _hModbus->ErrLatch);
    // _hModbus->mb_ops->SendMessage(ucDMATxBuf, ARRAYSIZE(ucDMATxBuf));
    _hModbus->mb_ops->SendMessage(ucDMATxBuf, 5);
	
    sprintf(ucTxBuf, "ERROR0%X", _hModbus->ErrLatch);
	bsp_JLXLcdClearLine(12, 2, 0x00);
    bsp_JLXLcdShowString(0, 12, ucTxBuf, 0, 0);
}

static void vResetSlaveRxState(BSP_ModbusHandleTypedef *_hModbus)
{
    // _hModbus->pass = 0;
    _hModbus->RxCnt = 0;
    _hModbus->exe = 0;
    _hModbus->ErrLatch = MODBUS_DEFAULT;
    _hModbus->CRCRx = 0xFFFF;

    _hModbus->DataByteLen = 0;
}
/* ===========================================主循环执行=========================================== */

/* ===========================================接收处理=========================================== */
static BSP_ModbusErrorTypeDef eMBSlaveReceive(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{    
    /* 存储接收状态机序列位置 */
    static uint16_t usRxReg = 0;

    _hModbus->pcRxBuffer[_hModbus->RxCnt] = _ucChar;   /* 接收字节流 */

    switch (_hModbus->RxCnt++) {   // TODO: Switch中的_ulRxCnt变量当前值为多少, 是++之前还是++之后
        case 0: {
            if (MODBUS_SLAVE_ADDR != _ucChar) {
                return MODBUS_PASS;
            }
            break;
        }
        case 1: {
            /* check validation of cmdcode */
            if (eCheckCmdCode(_hModbus, _ucChar) != BSP_OK) {
                return MODBUS_ERROR0A;
            }
            /* save Cmd code */
        } break;
        case 2: usRxReg = (usRxReg << 8) | _ucChar; break;
        case 3: {
            usRxReg = (usRxReg << 8) | _ucChar;
            if (eCheckRegAddr(_hModbus, usRxReg) != BSP_OK) {
                return MODBUS_ERROR02;
            } 
        } break;
        case 4: usRxReg = (usRxReg << 8) | _ucChar; break;
        case 5: {
            usRxReg = (usRxReg << 8) | _ucChar;
            if (eCheckRegLen(_hModbus, usRxReg) != BSP_OK) {
                return MODBUS_ERROR03;
            } 
        } break;
        default : break;
    }

    return MODBUS_DEFAULT;
}

static BSP_StatusTypedef eCheckCmdCode(BSP_ModbusHandleTypedef *_hModbus, uint8_t _ucChar)
{
    int i = 0;
    for (i = 0; i < _hModbus->CmdLen; i++) {
        if (_ucChar == _hModbus->pcCmd[i]) {
            _hModbus->CmdIndex = i;
            return BSP_OK;
        }
    }
    
    return BSP_ERROR;
}

static BSP_StatusTypedef eCheckRegAddr(BSP_ModbusHandleTypedef *_hModbus, uint16_t _usAddr)
{
    int i, j;
    for (i = 0; i < _hModbus->BaseAddrCnt; i++) {
        for (j = 0; j < _hModbus->psRegAddrLen[i]; j++) {
            if (_usAddr == (_hModbus->psBaseAddr[i] + j)) {
                _hModbus->BaseAddrIndex = i;
                _hModbus->RegAddrOffset = j;
                return BSP_OK;
            }
        }
    }

    return BSP_ERROR;
}

static BSP_StatusTypedef eCheckRegLen(BSP_ModbusHandleTypedef *_hModbus, uint16_t _usCnt)
{   /* XXX: 不同的命令，帧格式不同，如06H写单个数据命令，帧格式中没有参数值 */
    if (_hModbus->pcCmd[_hModbus->CmdIndex] != 0x06) {  /* FIXME: 如何将该判断的耦合性降低，提升命令的兼容性 */
        if ((_hModbus->RegAddrOffset + _usCnt - 1) >= _hModbus->psRegAddrLen[_hModbus->BaseAddrIndex]) {
            return BSP_ERROR;
        }
    }

    return BSP_OK;
}

static void vMakeCRC16Table(void)
{
    #define POLY 0xA001
    uint16_t c;
    int i, j;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) {
            if(c & 0x1) {   // 若最低位为1
                c = (c >> 1) ^ POLY;    // 与生成多项式异或
            } else {
                c = c >> 1;
            }
        }
        CRC16LUT[i] = c;    // 将计算得到的CRC校验码存入表中
    }
}

uint16_t bsp_MBCRC16(uint8_t *data, uint32_t len)
{
    uint16_t usCRC = 0xffff;  
    uint8_t index;
    while (len--) {
        index = (usCRC & 0xff) ^ *(data++);
        usCRC >>= 8;
        usCRC ^= CRC16LUT[index];
    }
    return usCRC;
}

static uint16_t usCRC16Stream(uint16_t _usLastCRC, uint8_t data)
{
    uint8_t index;

    index = (_usLastCRC & 0xff) ^ data;

    return ((_usLastCRC >> 8) ^ CRC16LUT[index]);
}
/* ===========================================接收处理=========================================== */

/* ===========================================命令执行=========================================== */
static BSP_ModbusErrorTypeDef vCmdFun01(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        _hModbus->pcTxBuffer[2] = _hModbus->pcTxBuffer[0];
        _hModbus->pcTxBuffer[3] = _hModbus->pcTxBuffer[1];
        _hModbus->pcTxBuffer[0] = _hModbus->SlaveAddr;
        _hModbus->pcTxBuffer[1] = _hModbus->pcCmd[_hModbus->CmdIndex];
        _hModbus->pcTxBuffer[4] = ((_hModbus->DataByteLen/2)>>8) & 0xFF;
        _hModbus->pcTxBuffer[5] = (_hModbus->DataByteLen/2) & 0xFF;
        _hModbus->TxCnt = 6;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* TODO: SLAVE_CMD_HANDLE */
        __NOP();
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef vCmdFun03(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        _hModbus->pcTxBuffer[2] = _hModbus->pcTxBuffer[0];
        _hModbus->pcTxBuffer[3] = _hModbus->pcTxBuffer[1];
        _hModbus->pcTxBuffer[0] = _hModbus->SlaveAddr;
        _hModbus->pcTxBuffer[1] = _hModbus->pcCmd[_hModbus->CmdIndex];
        _hModbus->pcTxBuffer[4] = ((_hModbus->DataByteLen/2)>>8) & 0xFF;
        _hModbus->pcTxBuffer[5] = (_hModbus->DataByteLen/2) & 0xFF;
        _hModbus->TxCnt = 6;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* 检查报文正确性 */
        if (_hModbus->RxCnt != 8) {
            return MODBUS_ERROR0B;  /* 请求或响应的长度与协议规定不符 */
        }
        
        /* 帧格式合法，执行参数基地址函数回调 */
        _hModbus->DataByteLen = (_hModbus->pcRxBuffer[4]<<8) + _hModbus->pcRxBuffer[5];
        _hModbus->DataByteLen *= 2;
        if (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex] != NULL) {
            return (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex](_hModbus, MB_READREG));
        }

        return MODBUS_ERROR0A;
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef vCmdFun05(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        _hModbus->pcTxBuffer[2] = _hModbus->pcTxBuffer[0];
        _hModbus->pcTxBuffer[3] = _hModbus->pcTxBuffer[1];
        _hModbus->pcTxBuffer[0] = _hModbus->SlaveAddr;
        _hModbus->pcTxBuffer[1] = _hModbus->pcCmd[_hModbus->CmdIndex];
        _hModbus->pcTxBuffer[4] = _hModbus->pcDataBuffer[1];
        _hModbus->pcTxBuffer[5] = _hModbus->pcDataBuffer[0];
        _hModbus->TxCnt = 6;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* TODO: SLAVE_CMD_HANDLE */
        __NOP();
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef vCmdFun06(BSP_ModbusHandleTypedef *_hModbus)
{
    if (_hModbus->Mode == MB_MASTER) {
        _hModbus->pcTxBuffer[2] = _hModbus->pcTxBuffer[0];
        _hModbus->pcTxBuffer[3] = _hModbus->pcTxBuffer[1];
        _hModbus->pcTxBuffer[0] = _hModbus->SlaveAddr;
        _hModbus->pcTxBuffer[1] = _hModbus->pcCmd[_hModbus->CmdIndex];
        _hModbus->pcTxBuffer[4] = _hModbus->pcDataBuffer[1];
        _hModbus->pcTxBuffer[5] = _hModbus->pcDataBuffer[0];
        _hModbus->TxCnt = 6;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* 检查报文正确性 */
        if (_hModbus->RxCnt != 8) {
            return MODBUS_ERROR0B;  /* 请求或响应的长度与协议规定不符 */
        }

        /* 帧格式合法，执行参数基地址函数回调 */
        _hModbus->DataByteLen = 2;
        _hModbus->pcDataBuffer = _hModbus->pcRxBuffer + 4;
        if (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex] != NULL) {
            return (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex](_hModbus, MB_WRITEREG));
        }

        return MODBUS_ERROR0A;
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef vCmdFun10(BSP_ModbusHandleTypedef *_hModbus)
{
    int i;  /* FIXME： 可以把此处的i换成_hModbus->TxCnt */
	
    if (_hModbus->Mode == MB_MASTER) {
        _hModbus->pcTxBuffer[2] = _hModbus->pcTxBuffer[0];
        _hModbus->pcTxBuffer[3] = _hModbus->pcTxBuffer[1];
        _hModbus->pcTxBuffer[0] = _hModbus->SlaveAddr;
        _hModbus->pcTxBuffer[1] = _hModbus->pcCmd[_hModbus->CmdIndex];
        _hModbus->pcTxBuffer[4] = ((_hModbus->DataByteLen/2)>>8) & 0xFF;
        _hModbus->pcTxBuffer[5] = (_hModbus->DataByteLen/2) & 0xFF;
        _hModbus->pcTxBuffer[6] = _hModbus->DataByteLen;
        for (i = 0; i < _hModbus->DataByteLen/2; i++) {
            _hModbus->pcTxBuffer[7 + 2*i]   = _hModbus->pcDataBuffer[2*i+1];
            _hModbus->pcTxBuffer[7 + 2*i+1] = _hModbus->pcDataBuffer[2*i];
        }
        i = i*2+7;
        _hModbus->TxCnt = i;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* 检查报文正确性 */
        if (_hModbus->pcRxBuffer[6] % 2) {  // 数据区字节数为奇数
            return MODBUS_ERROR0B;  /* 请求或响应的长度与协议规定不符 */
        } else if (_hModbus->pcRxBuffer[6] != (_hModbus->RxCnt-9)) {    // 检查写命令中，接收到的数据是否与声明的数据区长度匹配
            return MODBUS_ERROR03;  /* 请求的数据区字节与声明长度不符 */
        }

        /* 帧格式合法，执行参数基地址函数回调 */
        _hModbus->DataByteLen = _hModbus->pcRxBuffer[6];
        _hModbus->pcDataBuffer = _hModbus->pcRxBuffer + 7;
        if (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex] != NULL) {
            return (_hModbus->eBaseAddrCallback[_hModbus->BaseAddrIndex](_hModbus, MB_WRITEREG));
        }

        return MODBUS_ERROR0A;
    }
    
    return MODBUS_DEFAULT;
}
/* ===========================================命令执行=========================================== */

/* ===========================================命令应答=========================================== */
/* TODO: 从机TCP消息回复需要进行改写 */
static BSP_ModbusErrorTypeDef eCmdRSP01(BSP_ModbusHandleTypedef *_hModbus)
{
    uint32_t i;
    if (_hModbus->Mode == MB_MASTER) {
        if ((_hModbus->RxCnt-5+(_hModbus->CRCRx&2)) != _hModbus->pcRxBuffer[2]) {
            return MODBUS_ACKERR;
        }
        _hModbus->DataByteLen = _hModbus->pcRxBuffer[2];
        for (i = 0; i < _hModbus->DataByteLen; i++) {
            _hModbus->pcDataBuffer[i] = _hModbus->pcRxBuffer[3+i];
        }
        _hModbus->DataByteLen *= 2; /* GetRSP函数传出具体字节数而非数据数量 */
        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* TODO: SLAVEHandle */
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef eCmdRSP03(BSP_ModbusHandleTypedef *_hModbus)
{
    /* TODO: 不定长数据进行DMA发送，需要建堆，然后在发送完毕后释放 */
    char ucTxBuf[32] = {0};
    uint16_t usCRC = 0;
    uint32_t i;

    if (_hModbus->Mode == MB_MASTER) {
        if (((_hModbus->RxCnt-5+(_hModbus->CRCRx&2)) != _hModbus->pcRxBuffer[2]) || (_hModbus->pcRxBuffer[2]%2)) {
            return MODBUS_ACKERR;
        }

        _hModbus->DataByteLen = _hModbus->pcRxBuffer[2];
        for (i = 0; i < _hModbus->DataByteLen/2; i++) {
            _hModbus->pcDataBuffer[i*2] = _hModbus->pcRxBuffer[3+ i*2+1];
            _hModbus->pcDataBuffer[i*2+1] = _hModbus->pcRxBuffer[3 + i*2];
        }

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        _hModbus->pcTxBuffer[0] = MODBUS_SLAVE_ADDR;
        _hModbus->pcTxBuffer[1] = 0x03;

        _hModbus->pcTxBuffer[2] = (uint8_t)_hModbus->DataByteLen;   /* 数据区字节长度 */    
        for (i = 0; i < _hModbus->DataByteLen; i++) {   /* FIXME: 修复了数据发送格式错误的bug */
            _hModbus->pcTxBuffer[i+3] = _hModbus->pcDataBuffer[i];
        }
        i += 3;
        usCRC = bsp_MBCRC16(_hModbus->pcTxBuffer, i);
        _hModbus->pcTxBuffer[i] = usCRC & 0xFF;
        _hModbus->pcTxBuffer[i+1] = (usCRC>>8) & 0xFF;
        i += 2;

        _hModbus->mb_ops->SendMessage(_hModbus->pcTxBuffer, i);

        bsp_JLXLcdClearLine(12, 2, 0x00);
        sprintf(ucTxBuf, "03H + %XH: RxSUCESS", _hModbus->psBaseAddr[_hModbus->BaseAddrIndex] + _hModbus->RegAddrOffset);
        bsp_JLXLcdShowString(0, 12, ucTxBuf, 0, 0);

        return MODBUS_SUCESS;
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef eCmdRSP05(BSP_ModbusHandleTypedef *_hModbus)
{
    uint32_t i;
    if (_hModbus->Mode == MB_MASTER) {
        if ((_hModbus->RxCnt != _hModbus->TxCnt)) {
            return MODBUS_ACKERR;
        }

        for (i = 2; i < _hModbus->RxCnt; i++) {
            if (_hModbus->pcRxBuffer[i] != _hModbus->pcTxBuffer[i]) {
                return MODBUS_ACKERR;
            }
        }
        _hModbus->DataByteLen = 0;
        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        /* TODO: SLAVEHandle */
        __NOP();
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef eCmdRSP06(BSP_ModbusHandleTypedef *_hModbus)
{
	int i;
    char ucTxBuf[32] = {0};
    uint8_t *ucDMATxBuf = _hModbus->pcRxBuffer;

    if (_hModbus->Mode == MB_MASTER) {
        if ((_hModbus->RxCnt != _hModbus->TxCnt)) {
            return MODBUS_ACKERR;
        }

        for (i = 2; i < _hModbus->RxCnt; i++) {
            if (_hModbus->pcRxBuffer[i] != _hModbus->pcTxBuffer[i]) {
                return MODBUS_ACKERR;
            }
        }
        _hModbus->DataByteLen = 0;
        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        _hModbus->mb_ops->SendMessage(ucDMATxBuf, _hModbus->RxCnt);

        bsp_JLXLcdClearLine(12, 2, 0x00);
        sprintf(ucTxBuf, "06H + %XH: RxSUCESS", _hModbus->psBaseAddr[_hModbus->BaseAddrIndex] + _hModbus->RegAddrOffset);
        bsp_JLXLcdShowString(0, 12, ucTxBuf, 0, 0);
        return MODBUS_SUCESS;
    }

    return MODBUS_DEFAULT;
}

static BSP_ModbusErrorTypeDef eCmdRSP10(BSP_ModbusHandleTypedef *_hModbus)
{
    char ucTxBuf[32] = {0};
    uint16_t usCRC = 0;
    uint8_t i;

    if (_hModbus->Mode == MB_MASTER) {
        if (_hModbus->RxCnt != 8 - (_hModbus->CRCRx&2)) {
            return MODBUS_ACKERR;
        }

        for (i = 2; i < 6; i++) {
            if (_hModbus->pcRxBuffer[i] != _hModbus->pcTxBuffer[i]) {
                return MODBUS_ACKERR;
            }
        }
        
        _hModbus->DataByteLen = 0;

        return MODBUS_SUCESS;
    } else if (_hModbus->Mode == MB_SLAVE) {
        for (i = 0; i < 6; i++) {
            _hModbus->pcTxBuffer[i] = _hModbus->pcRxBuffer[i];
        }
        usCRC = bsp_MBCRC16(_hModbus->pcTxBuffer, i);
        _hModbus->pcTxBuffer[i] = usCRC & 0xFF;
        _hModbus->pcTxBuffer[i+1] = (usCRC>>8) & 0xFF;
        i += 2;

        _hModbus->mb_ops->SendMessage(_hModbus->pcTxBuffer, i);

        bsp_JLXLcdClearLine(12, 2, 0x00);
        sprintf(ucTxBuf, "10H + %XH: RxSUCESS", _hModbus->psBaseAddr[_hModbus->BaseAddrIndex] + _hModbus->RegAddrOffset);
        bsp_JLXLcdShowString(0, 12, ucTxBuf, 0, 0);
        return MODBUS_SUCESS;
    }

    return MODBUS_DEFAULT;
}

/* ===========================================命令应答=========================================== */
