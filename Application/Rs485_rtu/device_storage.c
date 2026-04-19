/**
 * @file device_storage.c
 * @brief 基于哈希算法的离散地址缓存实现
 */

#include "device_storage.h"

#include <string.h>

typedef struct {
    uint16_t address;
    uint16_t value;
    uint8_t flags;
} HashCacheNode;

#define HASH_FLAG_VALID 0x01U
#define HASH_FLAG_BIT 0x02U

static HashCacheNode g_hash_cache[HASH_CACHE_SIZE];

static uint16_t hash_cache_index(uint16_t address, bool is_bit) {
    uint32_t seed = ((uint32_t)address * 131U) + (is_bit ? 1U : 0U);
    return (uint16_t)(seed % HASH_CACHE_SIZE);
}

static HashCacheNode* hash_cache_find(uint16_t address, bool is_bit, bool create_if_missing) {
    uint16_t start = hash_cache_index(address, is_bit);
    uint16_t index = start;
    uint16_t steps = 0;

    while (steps < HASH_CACHE_SIZE) {
        HashCacheNode* node = &g_hash_cache[index];
        bool is_valid = (node->flags & HASH_FLAG_VALID) != 0U;
        bool node_is_bit = (node->flags & HASH_FLAG_BIT) != 0U;

        if (!is_valid) {
            if (!create_if_missing) {
                return NULL;
            }

            node->address = address;
            node->value = 0U;
            node->flags = HASH_FLAG_VALID | (is_bit ? HASH_FLAG_BIT : 0U);
            return node;
        }

        if (node->address == address && node_is_bit == is_bit) {
            return node;
        }

        index = (uint16_t)((index + 1U) % HASH_CACHE_SIZE);
        steps++;
    }

    return NULL;
}

void HashCache_Init(void) {
    memset(g_hash_cache, 0, sizeof(g_hash_cache));
}

bool HashCache_RegisterBit(uint16_t address) {
    return hash_cache_find(address, true, true) != NULL;
}

bool HashCache_RegisterWord(uint16_t address) {
    return hash_cache_find(address, false, true) != NULL;
}

bool HashCache_GetBit(uint16_t address, bool* value) {
    HashCacheNode* node;

    if (value == NULL) {
        return false;
    }

    node = hash_cache_find(address, true, false);
    if (node == NULL) {
        return false;
    }

    *value = (node->value != 0U);
    return true;
}

bool HashCache_SetBit(uint16_t address, bool value) {
    HashCacheNode* node = hash_cache_find(address, true, true);
    if (node == NULL) {
        return false;
    }

    node->value = value ? 1U : 0U;
    return true;
}

bool HashCache_GetWord(uint16_t address, uint16_t* value) {
    HashCacheNode* node;

    if (value == NULL) {
        return false;
    }

    node = hash_cache_find(address, false, false);
    if (node == NULL) {
        return false;
    }

    *value = node->value;
    return true;
}

bool HashCache_SetWord(uint16_t address, uint16_t value) {
    HashCacheNode* node = hash_cache_find(address, false, true);
    if (node == NULL) {
        return false;
    }

    node->value = value;
    return true;
}

bool HashCache_GetDWord(uint16_t address, uint32_t* value) {
    uint16_t low;
    uint16_t high;

    if (value == NULL) {
        return false;
    }

    if (!HashCache_GetWord(address, &low) || !HashCache_GetWord((uint16_t)(address + 1U), &high)) {
        return false;
    }

    *value = ((uint32_t)high << 16) | low;
    return true;
}

bool HashCache_SetDWord(uint16_t address, uint32_t value) {
    return HashCache_SetWord(address, (uint16_t)(value & 0xFFFFU)) &&
           HashCache_SetWord((uint16_t)(address + 1U), (uint16_t)((value >> 16) & 0xFFFFU));
}

bool HashCache_HasBit(uint16_t address) {
    return hash_cache_find(address, true, false) != NULL;
}

bool HashCache_HasWord(uint16_t address) {
    return hash_cache_find(address, false, false) != NULL;
}

void HashCache_GetMemoryUsage(uint32_t* total_memory) {
    if (total_memory != NULL) {
        *total_memory = sizeof(g_hash_cache);
    }
}

void DeviceStorage_Init(void) {
    HashCache_Init();
}

bool DeviceStorage_GetBit(uint16_t address) {
    bool value = false;
    HashCache_GetBit(address, &value);
    return value;
}

bool DeviceStorage_SetBit(uint16_t address, bool value) {
    return HashCache_SetBit(address, value);
}

bool DeviceStorage_ToggleBit(uint16_t address) {
    bool value = false;
    if (!HashCache_GetBit(address, &value)) {
        value = false;
    }

    return HashCache_SetBit(address, !value);
}

bool DeviceStorage_GetWord(uint16_t address, uint16_t* value) {
    return HashCache_GetWord(address, value);
}

bool DeviceStorage_SetWord(uint16_t address, uint16_t value) {
    return HashCache_SetWord(address, value);
}
