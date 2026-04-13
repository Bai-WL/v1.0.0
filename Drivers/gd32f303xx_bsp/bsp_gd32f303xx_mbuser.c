/**
 * @file bsp_gd32f303xx_mbuser.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-07
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stddef.h>
#include "bsp_gd32f303xx_modbus.h"
#include "bsp_gd32f303xx_mbuser.h"

/* user macro */
#define REG_ADDR_LEN_1000H 2
#define REG_ADDR_LEN_2000H 4
#define REG_ADDR_LEN_3000H 1

/* user prototype */
static BSP_ModbusErrorTypeDef eBaseAddrFun1000 (BSP_ModbusHandleTypedef *, BSP_ModbusRegRWTypeDef _eRW);

/* ==================global variable================== */
uint16_t g_pBaseAddrSet[MODBUS_BASEADDR_COUNTS] = {
    0x1000, 
    0x2000, 
    0x3000
};   /* XXX: 是否需要根据不同参数地址的命令归属，判断错误 */

uint16_t g_pRegAddrLen[MODBUS_BASEADDR_COUNTS] = {
    REG_ADDR_LEN_1000H, 
    REG_ADDR_LEN_2000H, 
    REG_ADDR_LEN_3000H
};

eBaseAddrCallbackTypeDef g_pBaseAddrFunSet[MODBUS_BASEADDR_COUNTS] = {
    eBaseAddrFun1000,
    NULL
};
/* ==================global variable================== */

// _hModbus->DataByteLen = (((uint16_t) _hModbus->pcRxBuffer[4])<<8) + _hModbus->pcRxBuffer[5];   /* FXIME: 易错笔记 */

static uint16_t REG1000H[REG_ADDR_LEN_1000H] = {0xFF0A, 0x0B};
static uint8_t pcRegAnsBuffer[REG_ADDR_LEN_1000H * 2] = {0};
static BSP_ModbusErrorTypeDef eBaseAddrFun1000 (BSP_ModbusHandleTypedef *_hModbus, BSP_ModbusRegRWTypeDef _eRW)
{
    int i;
    uint16_t usDataReg = 0;
    uint8_t *pcReg = (uint8_t *) REG1000H;

    if (_eRW == MB_READREG) {
        for (i = 0; i < _hModbus->DataByteLen; i++) {
            pcRegAnsBuffer[i] = (i%2) ? pcReg[i-1] : pcReg[i+1];
        }
        _hModbus->pcDataBuffer = pcRegAnsBuffer;

        return MODBUS_SUCESS;
    } else if (_eRW == MB_WRITEREG) {
        for (i = 0; i < _hModbus->DataByteLen; i++) {
            usDataReg = (usDataReg<<8) + _hModbus->pcDataBuffer[i];
            if (i%2) {
                REG1000H[_hModbus->RegAddrOffset + i/2] = usDataReg;
            }
        }

        return MODBUS_SUCESS;
    }

    return MODBUS_ERROR0A;
}
