#ifndef __USER_MB_CONTROLLER_H
#define __USER_MB_CONTROLLER_H

#if defined(__CC_ARM)
#pragma anon_unions
#endif
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bsp_gd32f303xx_modbus.h"

#define MAX_PRIORITY_QUEUE_SIZE 8 // 优先队列深度）
#define MAX_NORMAL_QUEUE_SIZE 8   // 普通队列深度

#define CACHE_SIZE 32 ///< 寄存器缓存条目数

/// 加入任务
typedef enum
{
    REQ_ADD_OK,
    REQ_ADD_FAILURE,
} ReqCreat_State;

/// Modbus控制器状态枚举
typedef enum
{
    MB_IDLE,       // 空闲状态
    MB_READING,    // 正在发送读请求
    MB_WRITING,    // 正在发送写请求
    MB_WAITING_RSP // 等待从机响应
} MB_State;

typedef enum
{
    MENU_TYPE_VALUE_INT = 0,
    MENU_TYPE_VALUE_DINT,
    MENU_TYPE_VALUE_BIT,
    MENU_TYPE_VALUE_UINT16,
    MENU_TYPE_VALUE_UINT32,
} MenuItemType;

/// Modbus请求类型枚举
typedef enum
{
    REQ_READ_COILS = 0x01,              ///< 读取线圈 (功能码 0x01)
    REQ_READ_DISCRETE_INPUTS = 0x02,    ///< 读取离散输入 (功能码 0x02)
    REQ_READ_HOLDING_REGISTERS = 0x03,  ///< 读取保持寄存器 (功能码 0x03)
    REQ_READ_INPUT_REGISTERS = 0x04,    ///< 读取输入寄存器 (功能码 0x04)
    REQ_WRITE_SINGLE_COIL = 0x05,       ///< 写单个线圈 (功能码 0x05)
    REQ_WRITE_SINGLE_REGISTER = 0x06,   ///< 写单个寄存器 (功能码 0x06)
    REQ_WRITE_MULTIPLE_COILS = 0x0F,    ///< 写多个线圈 (功能码 0x0F)
    REQ_WRITE_MULTIPLE_REGISTERS = 0x10 ///< 写多个寄存器 (功能码 0x10)
} MB_ReqType;

// Modbus请求结构体
typedef struct
{
    MB_ReqType type;    ///< 请求类型
    uint8_t slave_addr; ///< 从站地址
    uint16_t reg_addr;  ///< 寄存器起始地址
    union
    {
        struct
        { ///< 写入数据（写请求时使用）
            uint16_t *data;
            uint8_t len_of_data; // 数据长度
        };
        struct
        { ///< 读取长度（读请求时使用）
            uint8_t data_len;
        };
    };
} MB_Request;

/**
 * @brief 初始化Modbus控制器
 */
// void MBController_Init(void);

/**
 * @brief 处理队列中的Modbus请求
 */
void MBController_Process(BSP_ModbusHandleTypedef *_hModbus);

/**
 * @brief 添加Modbus请求到队列
 * @param req 要添加的请求结构体
 */
ReqCreat_State MBController_Request(MB_Request req);

/**
 * @brief 从缓存获取寄存器值
 * @param reg_addr 要查询的寄存器地址
 * @return 寄存器值，0xFFFF表示未找到
 */
bool MBController_GetCache(uint32_t reg_addr, uint16_t *value);
extern uint8_t rs_err; ///< 通信错误

#endif //__USER_MB_CONTROLLER_H
