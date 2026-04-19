/**
 * @file device_storage.h
 * @brief RS485 菜单联动使用的哈希缓存接口
 *
 * 文件名保持不变以兼容现有工程，但内部能力已经切换为哈希缓存。
 * 该缓存使用固定桶 + 线性探测管理离散的 Modbus 地址，避免为稀疏地址
 * 分配大块连续 RAM。
 */

#ifndef __DEVICE_STORAGE_H
#define __DEVICE_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// 常量定义
// ============================================================================

#define HASH_CACHE_SIZE 257U
#define HASH_CACHE_INVALID_ADDR 0xFFFFU

// ============================================================================
// 初始化函数
// ============================================================================

void HashCache_Init(void);

// ============================================================================
// 地址注册接口
// ============================================================================

bool HashCache_RegisterBit(uint16_t address);
bool HashCache_RegisterWord(uint16_t address);

// ============================================================================
// 读写接口
// ============================================================================

bool HashCache_GetBit(uint16_t address, bool* value);
bool HashCache_SetBit(uint16_t address, bool value);
bool HashCache_GetWord(uint16_t address, uint16_t* value);
bool HashCache_SetWord(uint16_t address, uint16_t value);
bool HashCache_GetDWord(uint16_t address, uint32_t* value);
bool HashCache_SetDWord(uint16_t address, uint32_t value);

// ============================================================================
// 状态查询
// ============================================================================

bool HashCache_HasBit(uint16_t address);
bool HashCache_HasWord(uint16_t address);
void HashCache_GetMemoryUsage(uint32_t* total_memory);

// 兼容层
// ============================================================================

void DeviceStorage_Init(void);
bool DeviceStorage_GetBit(uint16_t address);
bool DeviceStorage_SetBit(uint16_t address, bool value);
bool DeviceStorage_ToggleBit(uint16_t address);
bool DeviceStorage_GetWord(uint16_t address, uint16_t* value);
bool DeviceStorage_SetWord(uint16_t address, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICE_STORAGE_H */
