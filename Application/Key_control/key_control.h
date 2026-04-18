/**
 * @file key_control.h
 * @brief 按键控制系统 - 负责按键扫描、事件处理和导航状态管理
 * @version 1.0
 * @date 2026-04-18
 *
 * 功能说明：
 * 1. 按键扫描状态机，支持消抖、长按、重复触发
 * 2. 事件队列管理，避免事件丢失
 * 3. 原始按键值到标准功能键的映射
 * 4. 与菜单导航系统的集成接口
 */

#ifndef KEY_CONTROL_H
#define KEY_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// 数据类型定义
// ============================================================================

// 功能键映射枚举（与menu_system.h兼容）
typedef enum {
    FUNKEY_NONE = 0,   // 无按键
    FUNKEY_UP = 1,     // F1键 - 上
    FUNKEY_DOWN = 2,   // F2键 - 下
    FUNKEY_LEFT = 3,   // F3键 - 左/翻页
    FUNKEY_RIGHT = 4,  // F4键 - 右/翻页
    FUNKEY_ENTER = 5,  // F5键 - 确认/进入
    FUNKEY_ESC = 6     // F6键 - 取消/返回
} FunKeyMapping;

// 按键事件类型
typedef enum {
    KEY_EVENT_NONE = 0,    // 无事件
    KEY_EVENT_PRESS,       // 短按（按下后立即释放）
    KEY_EVENT_LONG_PRESS,  // 长按（按住超过1秒）
    KEY_EVENT_REPEAT       // 重复触发（长按后每100ms触发一次）
} KeyEventType;

// 按键事件结构
typedef struct {
    KeyEventType type;   // 事件类型
    FunKeyMapping key;   // 按键
    uint32_t timestamp;  // 时间戳（系统滴答）
} KeyEvent;

// 按键配置结构
typedef struct {
    uint32_t debounce_time;    // 消抖时间（ms）
    uint32_t long_press_time;  // 长按时间（ms）
    uint32_t repeat_delay;     // 重复触发延迟（ms）
    uint32_t repeat_interval;  // 重复触发间隔（ms）
} KeyConfig;

// 按键状态机状态
typedef enum {
    KEY_STATE_IDLE,          // 空闲状态
    KEY_STATE_PRESSED,       // 按下状态（正在消抖）
    KEY_STATE_HELD,          // 保持状态（消抖完成，等待长按判断）
    KEY_STATE_LONG_PRESSED,  // 长按状态
    KEY_STATE_RELEASED       // 释放状态
} KeyState;

// 原始按键值定义（与drv_hid.h中的hid_get_funkey()返回值对应）
#define RAW_KEY_NONE 0x00
#define RAW_KEY_F1 0x01
#define RAW_KEY_F2 0x02
#define RAW_KEY_F3 0x03
#define RAW_KEY_F4 0x04
#define RAW_KEY_F5 0x05
#define RAW_KEY_F6 0x06

// 事件队列大小
#define EVENT_QUEUE_SIZE 16

// ============================================================================
// API函数声明
// ============================================================================

// 初始化函数
void key_control_init(void);
void key_control_set_config(const KeyConfig* config);
void key_control_set_key_mapping(const FunKeyMapping* mapping);

// 按键扫描和处理
void key_control_scan(uint32_t current_tick);
bool key_control_get_event(KeyEvent* event);

// 状态查询
bool key_control_is_key_pressed(FunKeyMapping key);
uint32_t key_control_get_key_press_time(FunKeyMapping key);

// 工具函数
FunKeyMapping key_map_raw_to_function(uint8_t raw_key);
const char* key_event_type_to_string(KeyEventType type);
const char* key_mapping_to_string(FunKeyMapping key);

// 与菜单系统集成
void key_control_set_menu_callback(void (*callback)(FunKeyMapping, KeyEventType));

// 高级功能配置
void key_control_enable_long_press(bool enable);
void key_control_enable_repeat(bool enable);
void key_control_set_auto_repeat_interval(uint32_t interval);

#endif  // KEY_CONTROL_H
