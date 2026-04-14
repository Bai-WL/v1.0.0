#include "poll_manager.h"
#include "device_storage.h"
#include "screen_ids.h"
#include <string.h>

// 常驻地址列表（始终轮询，例如报警）
static const uint16_t alwaysAddrs[ALWAYS_POLL_ADDR_COUNT] = {12907}; // 假设时间写入地址
static const MenuItemType alwaysTypes[ALWAYS_POLL_ADDR_COUNT] = {MENU_TYPE_VALUE_INT};
static uint32_t alwaysLastValues[ALWAYS_POLL_ADDR_COUNT] = {0}; // 常驻地址的上次值

// 每个界面的地址列表
typedef struct
{
    uint16_t addr;
    MenuItemType type;
    uint32_t lastValue; // 上次值（用于比较是否更新）
} AddrEntry;

typedef struct
{
    AddrEntry entries[MAX_ADDR_PER_SCREEN];
    uint8_t count;
} ScreenAddrList;

static ScreenAddrList screenAddrs[MAX_SCREENS];
static bool screenRegistered[MAX_SCREENS] = {false}; // 注册标志
static uint8_t activeScreenId = SCREEN_ID_INVALID;   // 当前活动屏幕

void PollManager_Init(void)
{
    for (int i = 0; i < MAX_SCREENS; i++)
    {
        screenAddrs[i].count = 0;
        memset(screenAddrs[i].entries, 0, sizeof(screenAddrs[i].entries));
        screenRegistered[i] = false;
    }
    activeScreenId = SCREEN_ID_INVALID;
}

// 注册界面地址
void PollManager_RegisterScreenAddresses(uint8_t screenId, const uint16_t *addrs, const MenuItemType *types, uint8_t count)
{
    if (screenId < MAX_SCREENS)
    {
        activeScreenId = screenId;
    }
    else
    {
        activeScreenId = SCREEN_ID_INVALID;
        return;
    }
    if (screenRegistered[screenId])
        return; // 已注册，跳过

    if (count > MAX_ADDR_PER_SCREEN)
        count = MAX_ADDR_PER_SCREEN;

    ScreenAddrList *list = &screenAddrs[screenId];
    list->count = count;
    for (uint8_t i = 0; i < count; i++)
    {
        list->entries[i].addr = addrs[i];
        list->entries[i].type = types[i];
        list->entries[i].lastValue = 0XFFFFFFFF;
    }
    screenRegistered[screenId] = true; // 标记为已注册
}

// 设置当前活动屏幕
void PollManager_SetActiveScreen(uint8_t screenId)
{
    if (screenId < MAX_SCREENS)
    {
        activeScreenId = screenId;
    }
    else
    {
        activeScreenId = SCREEN_ID_INVALID;
    }
}

// 获取轮询地址列表（供 user_poll 调用）
uint8_t PollManager_GetPollingAddresses(uint16_t *addresses, MenuItemType *types, uint8_t maxCount)
{
    uint8_t idx = 0;

    // 1. 添加当前活动屏幕的地址
    if (activeScreenId < MAX_SCREENS)
    {
        ScreenAddrList *list = &screenAddrs[activeScreenId];
        for (uint8_t i = 0; i < list->count && idx < maxCount; i++)
        {
            addresses[idx] = list->entries[i].addr;
            types[idx] = list->entries[i].type;
            idx++;
        }
    }

    // 2. 添加常驻地址（去重）
    for (uint8_t i = 0; i < ALWAYS_POLL_ADDR_COUNT && idx < maxCount; i++)
    {
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
uint8_t PollManager_CheckAndUpdateValues(uint16_t *changedAddrs, uint32_t *changedValues)
{
    uint8_t changedCount = 0;

    // 获取当前需要扫描的地址列表
    uint8_t count = screenAddrs[activeScreenId].count + ALWAYS_POLL_ADDR_COUNT;

    for (uint8_t i = 0; i < count; i++)
    {
        uint32_t current;
        if (i < screenAddrs[activeScreenId].count)
        {
            if (screenAddrs[activeScreenId].entries[i].type == MENU_TYPE_VALUE_BIT)
            {
                current = DeviceStorage_GetBit(screenAddrs[activeScreenId].entries[i].addr) ? 1 : 0;
            }
            else if (screenAddrs[activeScreenId].entries[i].type == MENU_TYPE_VALUE_INT || screenAddrs[activeScreenId].entries[i].type == MENU_TYPE_VALUE_UINT16)
            {
                uint16_t val;
                if (DeviceStorage_GetWord(screenAddrs[activeScreenId].entries[i].addr, &val))
                {
                    current = val;
                }
                else
                {
                    continue; // 地址无效，跳过
                }
            }
            else if (screenAddrs[activeScreenId].entries[i].type == MENU_TYPE_VALUE_DINT || screenAddrs[activeScreenId].entries[i].type == MENU_TYPE_VALUE_UINT32)
            {
                uint16_t low, high;
                if (DeviceStorage_GetWord(screenAddrs[activeScreenId].entries[i].addr, &low) && DeviceStorage_GetWord(screenAddrs[activeScreenId].entries[i].addr + 1, &high))
                {
                    current = ((uint32_t)high << 16) | low; // 小端模式
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
            if (current != screenAddrs[activeScreenId].entries[i].lastValue)
            {
                // 更新lastValue
                screenAddrs[activeScreenId].entries[i].lastValue = current;
                // 记录变化的地址和值
                changedAddrs[changedCount] = screenAddrs[activeScreenId].entries[i].addr;
                changedValues[changedCount] = current;
                changedCount++;
            }
        }
        else
        {
            if (alwaysTypes[i - screenAddrs[activeScreenId].count] == MENU_TYPE_VALUE_BIT)
            {
                current = DeviceStorage_GetBit(alwaysAddrs[i - screenAddrs[activeScreenId].count]) ? 1 : 0;
            }
            else if (alwaysTypes[i - screenAddrs[activeScreenId].count] == MENU_TYPE_VALUE_INT || alwaysTypes[i - screenAddrs[activeScreenId].count] == MENU_TYPE_VALUE_UINT16)
            {
                uint16_t val;
                if (DeviceStorage_GetWord(alwaysAddrs[i - screenAddrs[activeScreenId].count], &val))
                {
                    current = val;
                }
                else
                {
                    continue; // 地址无效，跳过
                }
            }
            else if (alwaysTypes[i - screenAddrs[activeScreenId].count] == MENU_TYPE_VALUE_DINT || alwaysTypes[i - screenAddrs[activeScreenId].count] == MENU_TYPE_VALUE_UINT32)
            {
                uint16_t low, high;
                if (DeviceStorage_GetWord(alwaysAddrs[i - screenAddrs[activeScreenId].count], &low) && DeviceStorage_GetWord(alwaysAddrs[i - screenAddrs[activeScreenId].count] + 1, &high))
                {
                    current = ((uint32_t)high << 16) | low; // 小端模式
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
            if (current != alwaysLastValues[i - screenAddrs[activeScreenId].count])
            {
                // 更新lastValue
                alwaysLastValues[i - screenAddrs[activeScreenId].count] = current;
                // 记录变化的地址和值
                changedAddrs[changedCount] = alwaysAddrs[i - screenAddrs[activeScreenId].count];
                changedValues[changedCount] = current;
                changedCount++;
            }
        }
    }

    return changedCount;
}
