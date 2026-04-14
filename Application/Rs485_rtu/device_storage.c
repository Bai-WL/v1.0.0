/**
 * @file device_storage.c
 * @brief PLC装置存储系统实现
 * 
 * 实现位装置和字装置的存储管理，提供高效的访问接口。
 * 采用压缩存储方案减少内存占用，支持地址映射和批量操作。
 */

#include "device_storage.h"
#include <string.h>

// ============================================================================
// 全局变量定义
// ============================================================================

/** 位装置存储数组 - 使用位数组压缩存储 */
uint8_t bitDevices[(BIT_DEVICE_COUNT + 7) / 8] = {0};

/** 字装置存储数组 - 直接存储16位整数 */
uint16_t wordDevices[WORD_DEVICE_COUNT] = {0};

// ============================================================================
// 私有辅助函数
// ============================================================================

/**
 * @brief 计算位装置在数组中的字节索引和位偏移
 * 
 * @param address 位装置地址
 * @param[out] byte_index 字节索引
 * @param[out] bit_offset 位偏移 (0-7)
 * @return true 地址有效
 * @return false 地址无效
 */
static bool _get_bit_position(uint16_t address, uint16_t *byte_index, uint8_t *bit_offset)
{
    if (!IS_VALID_BIT_ADDRESS(address))
    {
        return false;
    }
    
    *byte_index = address / 8;
    *bit_offset = address % 8;
    return true;
}

/**
 * @brief 检查字装置地址有效性并计算索引
 * 
 * @param address 字装置地址
 * @param[out] index 数组索引
 * @return true 地址有效
 * @return false 地址无效
 */
static bool _get_word_index(uint16_t address, uint16_t *index)
{
    if (!IS_VALID_WORD_ADDRESS(address))
    {
        return false;
    }
    
    *index = WORD_ADDR_TO_INDEX(address);
    return true;
}

// ============================================================================
// 初始化函数
// ============================================================================

void DeviceStorage_Init(void)
{
    // 清零所有位装置
    memset(bitDevices, 0, sizeof(bitDevices));
    
    // 清零所有字装置
    memset(wordDevices, 0, sizeof(wordDevices));
    
    // 可以在这里添加初始化日志或调试信息
}

// ============================================================================
// 位装置访问接口实现
// ============================================================================

bool DeviceStorage_GetBit(uint16_t address)
{
    uint16_t byte_index;
    uint8_t bit_offset;
    
    if (!_get_bit_position(address, &byte_index, &bit_offset))
    {
        return false;
    }
    
    // 读取对应的位
    return (bitDevices[byte_index] >> bit_offset) & 0x01;
}

bool DeviceStorage_SetBit(uint16_t address, bool value)
{
    uint16_t byte_index;
    uint8_t bit_offset;
    
    if (!_get_bit_position(address, &byte_index, &bit_offset))
    {
        return false;
    }
    
    if (value)
    {
        // 设置位为1
        bitDevices[byte_index] |= (1 << bit_offset);
    }
    else
    {
        // 清除位为0
        bitDevices[byte_index] &= ~(1 << bit_offset);
    }
    
    return true;
}

bool DeviceStorage_ToggleBit(uint16_t address)
{
    uint16_t byte_index;
    uint8_t bit_offset;
    
    if (!_get_bit_position(address, &byte_index, &bit_offset))
    {
        return false;
    }
    
    // 切换位的值
    bitDevices[byte_index] ^= (1 << bit_offset);
    return true;
}

// ============================================================================
// 字装置访问接口实现
// ============================================================================

bool DeviceStorage_GetWord(uint16_t address, uint16_t *value)
{
    uint16_t index;
    
    if (!_get_word_index(address, &index) || value == NULL)
    {
        return false;
    }
    
    *value = wordDevices[index];
    return true;
}

bool DeviceStorage_SetWord(uint16_t address, uint16_t value)
{
    uint16_t index;
    
    if (!_get_word_index(address, &index))
    {
        return false;
    }
    
    wordDevices[index] = value;
    return true;
}

bool DeviceStorage_ModifyWord(uint16_t address, uint16_t mask, uint16_t value)
{
    uint16_t index;
    uint16_t current_value;
    
    if (!_get_word_index(address, &index))
    {
        return false;
    }
    
    // 读取当前值
    current_value = wordDevices[index];
    
    // 应用掩码：清除要修改的位，然后设置新值
    current_value = (current_value & ~mask) | (value & mask);
    
    // 写回
    wordDevices[index] = current_value;
    return true;
}

// ============================================================================
// 批量操作接口实现
// ============================================================================

bool DeviceStorage_GetBits(uint16_t start_address, uint16_t count, bool *buffer)
{
    if (buffer == NULL || count == 0)
    {
        return false;
    }
    
    // 检查地址范围
    if (start_address > BIT_DEVICE_COUNT || (start_address + count - 1) > BIT_DEVICE_COUNT)
    {
        return false;
    }
    
    for (uint16_t i = 0; i < count; i++)
    {
        buffer[i] = DeviceStorage_GetBit(start_address + i);
    }
    
    return true;
}

bool DeviceStorage_SetBits(uint16_t start_address, uint16_t count, const bool *buffer)
{
    if (buffer == NULL || count == 0)
    {
        return false;
    }
    
    // 检查地址范围
    if (start_address > BIT_DEVICE_COUNT || (start_address + count - 1) > BIT_DEVICE_COUNT)
    {
        return false;
    }
    
    for (uint16_t i = 0; i < count; i++)
    {
        if (!DeviceStorage_SetBit(start_address + i, buffer[i]))
        {
            return false;
        }
    }
    
    return true;
}

bool DeviceStorage_GetWords(uint16_t start_address, uint16_t count, uint16_t *buffer)
{
    if (buffer == NULL || count == 0)
    {
        return false;
    }
    
    // 检查地址范围
    if (start_address < WORD_DEVICE_START || 
        (start_address + count - 1) > WORD_DEVICE_END)
    {
        return false;
    }
    
    for (uint16_t i = 0; i < count; i++)
    {
        if (!DeviceStorage_GetWord(start_address + i, &buffer[i]))
        {
            return false;
        }
    }
    
    return true;
}

bool DeviceStorage_SetWords(uint16_t start_address, uint16_t count, const uint16_t *buffer)
{
    if (buffer == NULL || count == 0)
    {
        return false;
    }
    
    // 检查地址范围
    if (start_address < WORD_DEVICE_START || 
        (start_address + count - 1) > WORD_DEVICE_END)
    {
        return false;
    }
    
    for (uint16_t i = 0; i < count; i++)
    {
        if (!DeviceStorage_SetWord(start_address + i, buffer[i]))
        {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// 系统状态查询实现
// ============================================================================

void DeviceStorage_GetMemoryUsage(uint32_t *bit_memory, uint32_t *word_memory, uint32_t *total_memory)
{
    if (bit_memory != NULL)
    {
        *bit_memory = sizeof(bitDevices);
    }
    
    if (word_memory != NULL)
    {
        *word_memory = sizeof(wordDevices);
    }
    
    if (total_memory != NULL)
    {
        *total_memory = sizeof(bitDevices) + sizeof(wordDevices);
    }
}

bool DeviceStorage_ValidateAddress(uint16_t address, bool is_bit_device)
{
    if (is_bit_device)
    {
        return IS_VALID_BIT_ADDRESS(address);
    }
    else
    {
        return IS_VALID_WORD_ADDRESS(address);
    }
}

// ============================================================================
// 兼容性宏（可选，用于简化迁移）
// ============================================================================

/**
 * @defgroup 兼容性宏
 * @brief 提供与原始定义兼容的宏和函数
 * 
 * 这些宏和函数用于简化现有代码的迁移
 * @{
 */

/** 原始位装置访问宏（兼容性） */
#define get_bit_device(address) DeviceStorage_GetBit(address)
#define set_bit_device(address, value) DeviceStorage_SetBit(address, value)

/** 原始字装置访问宏（兼容性） */
#define get_word_device(address) (DeviceStorage_GetWord(address, &value), value)
#define set_word_device(address, value) DeviceStorage_SetWord(address, value)

/**
 * @brief 兼容性初始化函数
 * 
 * 如果现有代码调用DeviceStorage_Init有困难，可以使用此函数
 */
void init_device_storage(void)
{
    DeviceStorage_Init();
}

/** @} */ // end of 兼容性宏