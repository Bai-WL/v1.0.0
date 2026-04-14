#ifndef POLL_MANAGER_H
#define POLL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "user_mb_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

// 配置参数
#define MAX_SCREENS             10      // 最多10个界面
#define MAX_ADDR_PER_SCREEN     64      // 每个界面最多64个地址
#define ALWAYS_POLL_ADDR_COUNT  1       // 常驻地址数量（示例）

// 初始化轮询管理器
void PollManager_Init(void);

// 注册某界面的所有地址（由 Presenter 在 activate 时调用）
void PollManager_RegisterScreenAddresses(uint8_t screenId, const uint16_t* addrs, const MenuItemType* types, uint8_t count);

// 注销某界面的所有地址（由 Presenter 在 deactivate 时调用）
void PollManager_UnregisterScreenAddresses(uint8_t screenId);

// 获取当前需要轮询的地址列表（供 user_poll 调用）
uint8_t PollManager_GetPollingAddresses(uint16_t* addresses, MenuItemType* types, uint8_t maxCount);

// 检查并更新地址值，返回需要通知的地址数量
uint8_t PollManager_CheckAndUpdateValues(uint16_t* changedAddrs, uint32_t* changedValues);

#ifdef __cplusplus
}
#endif

#endif // POLL_MANAGER_H
