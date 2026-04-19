#include "poll_manager.h"

#include <string.h>

#include "device_storage.h"

// 常驻地址列表（始终轮询，例如报警）
static const uint16_t alwaysAddrs[ALWAYS_POLL_ADDR_COUNT] = {12907};  // 假设时间写入地址
static const MenuItemType alwaysTypes[ALWAYS_POLL_ADDR_COUNT] = {MENU_TYPE_VALUE_INT};
static uint32_t alwaysLastValues[ALWAYS_POLL_ADDR_COUNT] = {0};  // 常驻地址的上次值

// 每个界面的地址列表
typedef struct {
    uint16_t addr;
    MenuItemType type;
    uint32_t lastValue;  // 上次值（用于比较是否更新）
} AddrEntry;

typedef struct {
    AddrEntry entries[MAX_ADDR_PER_SCREEN];
    uint8_t count;
} ScreenAddrList;

static ScreenAddrList activeScreenAddrs;
static bool activeScreenRegistered = false;
static uint16_t activeScreenId = HASH_CACHE_INVALID_ADDR;  // 当前活动屏幕

static void PollManager_RegisterCacheAddress(uint16_t addr, MenuItemType type) {
    if (type == MENU_TYPE_VALUE_BIT) {
        HashCache_RegisterBit(addr);
    } else {
        HashCache_RegisterWord(addr);
        if (type == MENU_TYPE_VALUE_DINT || type == MENU_TYPE_VALUE_UINT32) {
            HashCache_RegisterWord((uint16_t)(addr + 1U));
        }
    }
}

static bool PollManager_ReadCachedValue(uint16_t addr, MenuItemType type, uint32_t* current) {
    bool bit_value;
    uint16_t word_value;
    uint32_t dword_value;

    if (current == NULL) {
        return false;
    }

    if (type == MENU_TYPE_VALUE_BIT) {
        if (!HashCache_GetBit(addr, &bit_value)) {
            return false;
        }

        *current = bit_value ? 1U : 0U;
        return true;
    }

    if (type == MENU_TYPE_VALUE_DINT || type == MENU_TYPE_VALUE_UINT32) {
        if (!HashCache_GetDWord(addr, &dword_value)) {
            return false;
        }

        *current = dword_value;
        return true;
    }

    if (!HashCache_GetWord(addr, &word_value)) {
        return false;
    }

    *current = word_value;
    return true;
}

void PollManager_Init(void) {
    memset(&activeScreenAddrs, 0, sizeof(activeScreenAddrs));
    activeScreenRegistered = false;
    activeScreenId = HASH_CACHE_INVALID_ADDR;
    for (uint8_t i = 0; i < ALWAYS_POLL_ADDR_COUNT; i++) {
        PollManager_RegisterCacheAddress(alwaysAddrs[i], alwaysTypes[i]);
    }
}

// 注册界面地址
void PollManager_RegisterScreenAddresses(uint16_t screenId, const uint16_t* addrs,
                                         const MenuItemType* types, uint8_t count) {
    if (count > MAX_ADDR_PER_SCREEN) {
        count = MAX_ADDR_PER_SCREEN;
    }

    ScreenAddrList* list = &activeScreenAddrs;
    memset(list->entries, 0, sizeof(list->entries));
    list->count = 0;
    for (uint8_t i = 0; i < count; i++) {
        bool duplicate = false;

        if (addrs == NULL || types == NULL) {
            break;
        }

        for (uint8_t j = 0; j < list->count; j++) {
            if (list->entries[j].addr == addrs[i] && list->entries[j].type == types[i]) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            continue;
        }

        list->entries[list->count].addr = addrs[i];
        list->entries[list->count].type = types[i];
        list->entries[list->count].lastValue = 0xFFFFFFFFU;
        PollManager_RegisterCacheAddress(addrs[i], types[i]);
        list->count++;
    }
    activeScreenRegistered = true;
    activeScreenId = screenId;
}

void PollManager_UnregisterScreenAddresses(uint16_t screenId) {
    (void)screenId;
    memset(&activeScreenAddrs, 0, sizeof(activeScreenAddrs));
    activeScreenRegistered = false;
    if (activeScreenId == screenId) {
        activeScreenId = HASH_CACHE_INVALID_ADDR;
    }
}

// 设置当前活动屏幕
void PollManager_SetActiveScreen(uint16_t screenId) {
    activeScreenId = screenId;
}

// 获取轮询地址列表（供 user_poll 调用）
uint8_t PollManager_GetPollingAddresses(uint16_t* addresses, MenuItemType* types,
                                        uint8_t maxCount) {
    uint8_t idx = 0;

    // 1. 添加当前活动屏幕的地址
    if (activeScreenRegistered) {
        ScreenAddrList* list = &activeScreenAddrs;
        for (uint8_t i = 0; i < list->count && idx < maxCount; i++) {
            addresses[idx] = list->entries[i].addr;
            types[idx] = list->entries[i].type;
            idx++;
        }
    }

    // 2. 添加常驻地址（去重）
    for (uint8_t i = 0; i < ALWAYS_POLL_ADDR_COUNT && idx < maxCount; i++) {
        // if (!isAddrAlreadyInList(alwaysAddrs[i], addresses, idx))
        // {
        addresses[idx] = alwaysAddrs[i];
        types[idx] = alwaysTypes[i];
        idx++;
        // }
    }

    return idx;
}

// 检查并更新地址值，返回需要通知的地址数量
uint8_t PollManager_CheckAndUpdateValues(uint16_t* changedAddrs, uint32_t* changedValues) {
    uint8_t changedCount = 0;
    uint8_t screenCount = 0;
    uint8_t count;

    if (activeScreenRegistered) {
        screenCount = activeScreenAddrs.count;
    }

    count = (uint8_t)(screenCount + ALWAYS_POLL_ADDR_COUNT);

    for (uint8_t i = 0; i < count; i++) {
        uint32_t current;
        if (i < screenCount) {
            if (!PollManager_ReadCachedValue(activeScreenAddrs.entries[i].addr,
                                             activeScreenAddrs.entries[i].type, &current)) {
                continue;
            }
            if (current != activeScreenAddrs.entries[i].lastValue) {
                // 更新lastValue
                activeScreenAddrs.entries[i].lastValue = current;
                // 记录变化的地址和值
                changedAddrs[changedCount] = activeScreenAddrs.entries[i].addr;
                changedValues[changedCount] = current;
                changedCount++;
            }
        } else {
            uint8_t alwaysIndex = (uint8_t)(i - screenCount);
            if (!PollManager_ReadCachedValue(alwaysAddrs[alwaysIndex], alwaysTypes[alwaysIndex],
                                             &current)) {
                continue;
            }
            if (current != alwaysLastValues[alwaysIndex]) {
                // 更新lastValue
                alwaysLastValues[alwaysIndex] = current;
                // 记录变化的地址和值
                changedAddrs[changedCount] = alwaysAddrs[alwaysIndex];
                changedValues[changedCount] = current;
                changedCount++;
            }
        }
    }

    return changedCount;
}
