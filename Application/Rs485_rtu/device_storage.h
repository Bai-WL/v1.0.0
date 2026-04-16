/**
 * @file device_storage.h
 * @brief PLC装置存储系统 - 提供位装置和字装置的统一存储接口
 * 
 * 本模块实现PLC装置的数据存储，包括：
 * 1. 位装置 (Bit Devices): 0-16383共16384个布尔值，使用位数组压缩存储
 * 2. 字装置 (Word Devices): 2914-12913共9999个16位整数，使用数组直接存储
 * 
 * 提供统一的访问接口，支持地址映射、数据读写和变更通知。
 */

#ifndef __DEVICE_STORAGE_H
#define __DEVICE_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// 常量定义
// ============================================================================

/** 位装置总数 (0-16383) */
#define BIT_DEVICE_COUNT 16383

/** 字装置总数 */
#define WORD_DEVICE_COUNT 9999

/** 字装置起始地址 */
#define WORD_DEVICE_START 2914

/** 字装置结束地址 */
#define WORD_DEVICE_END 12913

/** 检查地址是否为有效位装置地址 */
#define IS_VALID_BIT_ADDRESS(addr) ((addr) <= BIT_DEVICE_COUNT)

/** 检查地址是否为有效字装置地址 */
#define IS_VALID_WORD_ADDRESS(addr) ((addr) >= WORD_DEVICE_START && (addr) <= WORD_DEVICE_END)

/** 将字装置地址转换为数组索引 */
#define WORD_ADDR_TO_INDEX(addr) ((addr) - WORD_DEVICE_START)

/** 将数组索引转换为字装置地址 */
#define INDEX_TO_WORD_ADDR(index) ((index) + WORD_DEVICE_START)

// ============================================================================
// 外部变量声明
// ============================================================================

/** 
 * 位装置存储数组
 * 使用位数组压缩存储，每个字节存储8个位装置
 * 大小: (BIT_DEVICE_COUNT + 7) / 8 = 2048字节
 */
extern uint8_t bitDevices[(BIT_DEVICE_COUNT + 7) / 8];

/** 
 * 字装置存储数组
 * 直接存储16位整数值
 * 大小: WORD_DEVICE_COUNT * 2 = 19998字节
 */
extern uint16_t wordDevices[WORD_DEVICE_COUNT];

// ============================================================================
// 初始化函数
// ============================================================================

/**
 * @brief 初始化装置存储系统
 * 
 * 清除所有位装置和字装置的初始值（设置为0）
 * 应在系统启动时调用一次
 */
void DeviceStorage_Init(void);

// ============================================================================
// 位装置访问接口
// ============================================================================

/**
 * @brief 读取位装置的值
 * 
 * @param address 位装置地址 (0-16383)
 * @return true 位值为1
 * @return false 位值为0 或 地址无效
 */
bool DeviceStorage_GetBit(uint16_t address);

/**
 * @brief 设置位装置的值
 * 
 * @param address 位装置地址 (0-16383)
 * @param value 要设置的值 (true=1, false=0)
 * @return true 设置成功
 * @return false 地址无效，设置失败
 */
bool DeviceStorage_SetBit(uint16_t address, bool value);

/**
 * @brief 切换位装置的值
 * 
 * @param address 位装置地址 (0-16383)
 * @return true 切换成功
 * @return false 地址无效，切换失败
 */
bool DeviceStorage_ToggleBit(uint16_t address);

// ============================================================================
// 字装置访问接口
// ============================================================================

/**
 * @brief 读取字装置的值
 * 
 * @param address 字装置地址 (2914-12913)
 * @param[out] value 存储读取值的指针
 * @return true 读取成功
 * @return false 地址无效，读取失败
 */
bool DeviceStorage_GetWord(uint16_t address, uint16_t *value);

/**
 * @brief 设置字装置的值
 * 
 * @param address 字装置地址 (2914-12913)
 * @param value 要设置的16位整数值
 * @return true 设置成功
 * @return false 地址无效，设置失败
 */
bool DeviceStorage_SetWord(uint16_t address, uint16_t value);

/**
 * @brief 原子性地读取-修改-写入字装置
 * 
 * @param address 字装置地址 (2914-12913)
 * @param mask 要修改的位掩码
 * @param value 要设置的值（仅在mask为1的位上生效）
 * @return true 操作成功
 * @return false 地址无效，操作失败
 */
bool DeviceStorage_ModifyWord(uint16_t address, uint16_t mask, uint16_t value);

// ============================================================================
// 批量操作接口
// ============================================================================

/**
 * @brief 批量读取位装置
 * 
 * @param start_address 起始地址
 * @param count 要读取的数量
 * @param[out] buffer 输出缓冲区
 * @return true 全部读取成功
 * @return false 地址范围无效，部分或全部失败
 */
bool DeviceStorage_GetBits(uint16_t start_address, uint16_t count, bool *buffer);

/**
 * @brief 批量设置位装置
 * 
 * @param start_address 起始地址
 * @param count 要设置的数量
 * @param buffer 输入缓冲区
 * @return true 全部设置成功
 * @return false 地址范围无效，部分或全部失败
 */
bool DeviceStorage_SetBits(uint16_t start_address, uint16_t count, const bool *buffer);

/**
 * @brief 批量读取字装置
 * 
 * @param start_address 起始地址
 * @param count 要读取的数量
 * @param[out] buffer 输出缓冲区
 * @return true 全部读取成功
 * @return false 地址范围无效，部分或全部失败
 */
bool DeviceStorage_GetWords(uint16_t start_address, uint16_t count, uint16_t *buffer);

/**
 * @brief 批量设置字装置
 * 
 * @param start_address 起始地址
 * @param count 要设置的数量
 * @param buffer 输入缓冲区
 * @return true 全部设置成功
 * @return false 地址范围无效，部分或全部失败
 */
bool DeviceStorage_SetWords(uint16_t start_address, uint16_t count, const uint16_t *buffer);

// ============================================================================
// 系统状态查询
// ============================================================================

/**
 * @brief 获取装置存储系统的内存使用情况
 * 
 * @param[out] bit_memory 位装置内存使用（字节）
 * @param[out] word_memory 字装置内存使用（字节）
 * @param[out] total_memory 总内存使用（字节）
 */
void DeviceStorage_GetMemoryUsage(uint32_t *bit_memory, uint32_t *word_memory, uint32_t *total_memory);

/**
 * @brief 验证地址是否在有效范围内
 * 
 * @param address 要验证的地址
 * @param is_bit_device 是否为位装置（true=位装置，false=字装置）
 * @return true 地址有效
 * @return false 地址无效
 */
bool DeviceStorage_ValidateAddress(uint16_t address, bool is_bit_device);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICE_STORAGE_H */
