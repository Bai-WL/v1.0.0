#include "user_mb_controller.h"

#include "device_storage.h"

static MB_State mb_state = MB_IDLE;  ///< 当前通信状态

static MB_Request priority_queue[MAX_PRIORITY_QUEUE_SIZE];  // 优先队列（写请求
static uint8_t priority_head = 0;
static uint8_t priority_tail = 0;

static MB_Request normal_queue[MAX_NORMAL_QUEUE_SIZE];  // 普通队列（读请求
static uint8_t normal_head = 0;
static uint8_t normal_tail = 0;

static uint8_t timeoutcnt = 0;  ///< 通信超时次数
uint8_t rs_err = 0;

static uint8_t req_full_cnt = 0;  // 队列满标记
static uint8_t req_err_cnt = 0;   // 错误请求计数

// 初始化Modbus控制器和地址映射管理器
void MBController_Init(void) {}
/**
 * @brief 添加Modbus请求到队列
 * @param req 要添加的请求结构体
 * @return 请求添加状态
 *
 * 改进点：
 * 1. 使用新的枚举类型
 * 2. 更清晰的队列管理逻辑
 */
ReqCreat_State MBController_Request(MB_Request req) {
    ReqCreat_State creat_state = REQ_ADD_OK;

    // 判断请求类型：写请求进入优先队列，读请求进入普通队列
    if (req.type == REQ_WRITE_SINGLE_COIL || req.type == REQ_WRITE_SINGLE_REGISTER ||
        req.type == REQ_WRITE_MULTIPLE_REGISTERS) {
        // 添加到优先队列（写请求）
        if ((priority_tail + 1) % MAX_PRIORITY_QUEUE_SIZE != priority_head) {
            priority_queue[priority_tail] = req;
            priority_tail = (priority_tail + 1) % MAX_PRIORITY_QUEUE_SIZE;
        } else {
            req_full_cnt++;
            // 写请求队列满，需要调整队列大小
            // creat_state = REQ_ADD_FAILURE;
        }
    } else if (req.type == REQ_READ_COILS || req.type == REQ_READ_HOLDING_REGISTERS) {
        // 添加到普通队列（读请求）
        if ((normal_tail + 1) % MAX_NORMAL_QUEUE_SIZE != normal_head) {
            normal_queue[normal_tail] = req;
            normal_tail = (normal_tail + 1) % MAX_NORMAL_QUEUE_SIZE;
        } else {
            creat_state = REQ_ADD_FAILURE;
            req_full_cnt++;
        }
    } else {
        // 不支持的请求类型
        creat_state = REQ_ADD_FAILURE;
        req_err_cnt++;
    }

    return creat_state;
}

bool MBController_GetCache(uint32_t reg_addr, uint16_t* value) {
    return HashCache_GetWord((uint16_t)reg_addr, value);
}

void process_always_read_data(void) {}

/**
 * @brief 处理队列中的Modbus请求
 * @param _hModbus Modbus句柄
 */
void MBController_Process(BSP_ModbusHandleTypedef* _hModbus) {
    static uint16_t rx_buffer[64] = {0};     // 接收缓冲区
    static BSP_ModbusErrorTypeDef mb_error;  // Modbus错误状态
    static uint8_t rx_data_len;              // 接收数据长度
    static MB_Request current_req;           // 当前正在处理的请求

    // 仅在空闲状态时提取新请求
    if (mb_state == MB_IDLE) {
        // 优先处理优先队列（写请求）
        if (priority_head != priority_tail) {
            current_req = priority_queue[priority_head];
            priority_head = (priority_head + 1) % MAX_PRIORITY_QUEUE_SIZE;
            mb_state = MB_WRITING;
        }
        // 优先队列空则处理普通队列（读请求）
        else if (normal_head != normal_tail) {
            current_req = normal_queue[normal_head];
            normal_head = (normal_head + 1) % MAX_NORMAL_QUEUE_SIZE;
            mb_state = MB_READING;
        }
        // 队列全空则直接返回
        else {
            return;
        }
    }

    // 根据当前状态分发处理
    switch (mb_state) {
    case MB_WRITING: {
        // 准备写入数据
        uint16_t write_data[16] = {0};
        uint8_t write_len = 0;

        // 复制写入数据
        if (current_req.len_of_data > 0) {
            write_len = (current_req.len_of_data > 2U) ? 2U : current_req.len_of_data;
            for (uint8_t i = 0; i < write_len; i++) {
                write_data[i] = current_req.data_payload[i];
            }
        }

        // 根据请求类型发送相应的Modbus命令
        BSP_StatusTypedef status = BSP_ERROR;

        switch (current_req.type) {
        case REQ_WRITE_SINGLE_COIL:  // 功能码 0x05
            // 线圈值：0x0000 = OFF, 0xFF00 = ON
            if (write_len > 0) {
                write_data[0] = (write_data[0] != 0) ? 0xFF00 : 0x0000;
                status = bsp_MBMasterTransmitCommand(_hModbus, current_req.slave_addr, 0x05,
                                                     current_req.reg_addr, write_data, 1, &mb_error,
                                                     write_data, &rx_data_len);
            }
            break;

        case REQ_WRITE_SINGLE_REGISTER:  // 功能码 0x06
            if (write_len > 0) {
                status = bsp_MBMasterTransmitCommand(_hModbus, current_req.slave_addr, 0x06,
                                                     current_req.reg_addr, write_data, 1, &mb_error,
                                                     write_data, &rx_data_len);
            }
            break;

        case REQ_WRITE_MULTIPLE_REGISTERS:  // 功能码 0x10
            if (write_len > 0) {
                status = bsp_MBMasterTransmitCommand(_hModbus, current_req.slave_addr, 0x10,
                                                     current_req.reg_addr, write_data, write_len,
                                                     &mb_error, write_data, &rx_data_len);
            }
            break;

        default:
            // 不支持的写操作类型
            mb_state = MB_IDLE;
            return;
        }

        if (status == BSP_OK) {
            mb_state = MB_WAITING_RSP;
        } else {
            // 发送失败，重置状态
            // mb_state = MB_IDLE;
        }
        break;
    }

    case MB_READING: {
        // 根据请求类型发送相应的Modbus命令
        BSP_StatusTypedef status = BSP_ERROR;

        switch (current_req.type) {
        case REQ_READ_COILS:  // 功能码 0x01
            status = bsp_MBMasterTransmitCommand(_hModbus, current_req.slave_addr, 0x01,
                                                 current_req.reg_addr, NULL, current_req.data_len,
                                                 &mb_error, rx_buffer, &rx_data_len);
            break;

        case REQ_READ_HOLDING_REGISTERS:  // 功能码 0x03
            status = bsp_MBMasterTransmitCommand(_hModbus, current_req.slave_addr, 0x03,
                                                 current_req.reg_addr, NULL, current_req.data_len,
                                                 &mb_error, rx_buffer, &rx_data_len);
            break;

        default:
            // 不支持的读操作类型
            mb_state = MB_IDLE;
            return;
        }

        if (status == BSP_OK) {
            mb_state = MB_WAITING_RSP;
        } else {
            // 发送失败，重置状态
            // mb_state = MB_IDLE;
        }
        break;
    }

    case MB_WAITING_RSP: {
        // 检查Modbus操作是否完成
        if (mb_error != MODBUS_BUSY) {
            // 处理操作结果
            switch (mb_error) {
            case MODBUS_SUCESS:
                // 操作成功
                timeoutcnt = 0;
                rs_err = 0;

                // 处理读取的数据
                if (current_req.type == REQ_READ_COILS ||
                    current_req.type == REQ_READ_HOLDING_REGISTERS) {
                    // 更新缓存
                    // memset(reg_cache, 0, sizeof(reg_cache));

                    if (current_req.type == REQ_READ_COILS) {
                        // 处理线圈数据（位数据）
                        uint8_t byte_index = 0;
                        uint8_t bit_index = 0;

                        for (uint16_t i = 0; i < current_req.data_len; i++) {
                            // 计算字节和位索引
                            byte_index = i / 8;
                            bit_index = i % 8;

                            if (byte_index < rx_data_len) {
                                // 提取位值
                                uint8_t byte_value = (uint8_t)(rx_buffer[byte_index] & 0xFF);
                                uint16_t coil_value = (byte_value >> bit_index) & 0x01;

                                // 更新缓存
                                uint16_t cache_addr = current_req.reg_addr + i;

                                HashCache_SetBit(cache_addr, (coil_value != 0));
                            }
                        }
                    } else  // REQ_READ_HOLDING_REGISTERS
                    {
                        // 处理寄存器数据（字数据）
                        for (uint8_t i = 0; i < rx_data_len; i++) {
                            // 更新缓存
                            uint16_t cache_addr = current_req.reg_addr + i;

                            HashCache_SetWord(cache_addr, rx_buffer[i]);
                        }
                    }

                    // 清空接收缓冲区
                    rx_data_len = 0;
                    memset(rx_buffer, 0, sizeof(rx_buffer));
                }
                // 写成功后同步更新本地哈希缓存，保证 UI 立即可见。
                if (current_req.type == REQ_WRITE_SINGLE_COIL ||
                    current_req.type == REQ_WRITE_SINGLE_REGISTER ||
                    current_req.type == REQ_WRITE_MULTIPLE_REGISTERS) {
                    if (current_req.type == REQ_WRITE_SINGLE_COIL) {
                        HashCache_SetBit(current_req.reg_addr, current_req.data_payload[0] != 0U);
                    } else if (current_req.type == REQ_WRITE_SINGLE_REGISTER) {
                        HashCache_SetWord(current_req.reg_addr, current_req.data_payload[0]);
                    } else {
                        for (uint8_t i = 0; i < current_req.len_of_data; i++) {
                            HashCache_SetWord((uint16_t)(current_req.reg_addr + i),
                                              current_req.data_payload[i]);
                        }
                    }
                }
                // 重置状态
                mb_state = MB_IDLE;
                break;

            case MODBUS_NACK:    // 从机无应答
            case MODBUS_ACKERR:  // 从机应答错误
            case MODBUS_ERRACK:  // 从机错误应答
                // 通信错误，增加超时计数
                timeoutcnt++;

                // 如果重试次数超过限制，记录错误并重置状态
                if (timeoutcnt > 5) {
                    rs_err = (mb_error == MODBUS_NACK) ? 1 : (mb_error == MODBUS_ACKERR) ? 2 : 3;
                    mb_state = MB_IDLE;
                } else {
                    // 重试当前请求
                    if (current_req.type == REQ_WRITE_SINGLE_COIL ||
                        current_req.type == REQ_WRITE_SINGLE_REGISTER ||
                        current_req.type == REQ_WRITE_MULTIPLE_REGISTERS) {
                        mb_state = MB_WRITING;
                    } else if (current_req.type == REQ_READ_COILS ||
                               current_req.type == REQ_READ_HOLDING_REGISTERS) {
                        mb_state = MB_READING;
                    } else {
                        mb_state = MB_IDLE;
                    }
                }
                break;

            default:
                // 其他错误（从机返回的错误码）
                rs_err = 16;  // 通用错误码
                mb_state = MB_IDLE;
                break;
            }
        }
        break;
    }

    default:
        break;
    }
}
