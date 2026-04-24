/**
 * @file key_control.c
 * @brief 按键控制系统实现
 * @version 1.0
 * @date 2026-04-18
 *
 * 实现功能：
 * 1. 按键扫描状态机（消抖、长按、重复触发）
 * 2. 事件队列管理
 * 3. 原始按键值到标准功能键的映射
 * 4. 与菜单系统的集成接口
 */

#include "key_control.h"

#include <string.h>

// 外部依赖
#include "bsp_gd32f303xx_systick.h"
#include "drv_hid.h"

// ============================================================================
// 内部类型定义
// ============================================================================

// 按键状态机上下文
typedef struct {
    KeyState state;             // 当前状态
    uint32_t press_start_time;  // 按键按下开始时间
    uint32_t last_repeat_time;  // 上次重复触发时间
    uint8_t raw_key_value;      // 原始按键值
    FunKeyMapping mapped_key;   // 映射后的功能键
    bool is_active;             // 是否处于活动状态
} KeyContext;

// 事件队列结构
typedef struct {
    KeyEvent events[EVENT_QUEUE_SIZE];  // 事件数组
    uint8_t head;                       // 队列头指针
    uint8_t tail;                       // 队列尾指针
    uint8_t count;                      // 当前事件数量
} EventQueue;

// ============================================================================
// 内部全局变量
// ============================================================================

// 默认按键配置
static KeyConfig key_config = {
    .debounce_time = 20,      // 20ms消抖
    .long_press_time = 1000,  // 1秒长按
    .repeat_delay = 500,      // 长按500ms后开始重复
    .repeat_interval = 100    // 每100ms重复一次
};

// 按键映射表（默认映射：F1->UP, F2->DOWN, F3->LEFT, F4->RIGHT, F5->ENTER, F6->ESC）
static FunKeyMapping key_mapping[6] = {
    FUNKEY_LEFT,   // F1
    FUNKEY_RIGHT,  // F2
    FUNKEY_ESC,    // F5
    FUNKEY_DOWN,  // F4
    FUNKEY_UP,    // F3
    FUNKEY_ENTER,  // F6
};

// 原始按键表，索引与key_mapping、key_contexts保持一致
static const uint8_t raw_key_table[6] = {RAW_KEY_F1, RAW_KEY_F2, RAW_KEY_F3,
                                         RAW_KEY_F4, RAW_KEY_F5, RAW_KEY_F6};

// 按键上下文数组（每个功能键一个上下文）
static KeyContext key_contexts[6];

// 事件队列
static EventQueue event_queue;

// 功能使能标志
static bool long_press_enabled = true;
static bool repeat_enabled = true;

// ============================================================================
// 内部辅助函数
// ============================================================================

/**
 * @brief 初始化按键上下文
 */
static void init_key_contexts(void) {
    for (int i = 0; i < 6; i++) {
        key_contexts[i].state = KEY_STATE_IDLE;
        key_contexts[i].press_start_time = 0;
        key_contexts[i].last_repeat_time = 0;
        key_contexts[i].raw_key_value = 0;
        key_contexts[i].mapped_key = FUNKEY_NONE;
        key_contexts[i].is_active = false;
    }
}

/**
 * @brief 初始化事件队列
 */
static void init_event_queue(void) {
    event_queue.head = 0;
    event_queue.tail = 0;
    event_queue.count = 0;
}

/**
 * @brief 向事件队列添加事件
 * @param event 要添加的事件
 * @return true 添加成功，false 队列已满
 */
static bool event_queue_push(KeyEvent* event) {
    if (event_queue.count >= EVENT_QUEUE_SIZE) {
        // // 队列满时，丢弃最旧的事件（循环覆盖）
        // event_queue.head = (event_queue.head + 1) % EVENT_QUEUE_SIZE;
        // event_queue.count--;
        return false;
    }

    event_queue.events[event_queue.tail] = *event;
    event_queue.tail = (event_queue.tail + 1) % EVENT_QUEUE_SIZE;
    event_queue.count++;

    return true;
}

/**
 * @brief 从事件队列获取事件
 * @param event 接收事件的指针
 * @return true 获取成功，false 队列为空
 */
static bool event_queue_pop(KeyEvent* event) {
    if (event_queue.count == 0) {
        return false;
    }

    *event = event_queue.events[event_queue.head];
    event_queue.head = (event_queue.head + 1) % EVENT_QUEUE_SIZE;
    event_queue.count--;

    return true;
}

/**
 * @brief 统一派发按键事件到队列
 */
static void dispatch_key_event(FunKeyMapping key, KeyEventType type, uint32_t current_tick) {
    KeyEvent event = {.type = type, .key = key, .timestamp = current_tick};

    event_queue_push(&event);
}

/**
 * @brief 获取原始按键值的位索引
 * @param raw_key 原始按键值
 * @return 位索引（0-5），-1表示无效按键
 */
static int8_t get_raw_key_index(uint8_t raw_key) {
    switch (raw_key) {
    case RAW_KEY_F1:
        return 0;
    case RAW_KEY_F2:
        return 1;
    case RAW_KEY_F3:
        return 2;
    case RAW_KEY_F4:
        return 3;
    case RAW_KEY_F5:
        return 4;
    case RAW_KEY_F6:
        return 5;
    default:
        return -1;
    }
}

/**
 * @brief 处理按键状态机
 * @param ctx 按键上下文
 * @param current_tick 当前系统滴答
 */
static void process_key_state_machine(KeyContext* ctx, uint32_t current_tick) {
    switch (ctx->state) {
    case KEY_STATE_IDLE:
        // 空闲状态，等待按键按下
        if (ctx->raw_key_value != 0) {
            ctx->state = KEY_STATE_PRESSED;
            ctx->press_start_time = current_tick;
        }
        break;

    case KEY_STATE_PRESSED:
        // 按下状态，进行消抖
        if (ctx->raw_key_value == 0) {
            // 按键释放，回到空闲状态
            ctx->state = KEY_STATE_IDLE;
        } else if (current_tick - ctx->press_start_time >= key_config.debounce_time) {
            // 消抖完成，进入保持状态
            ctx->state = KEY_STATE_HELD;

            // 生成长按事件（如果长按功能被禁用，则生成短按事件）
            if (!long_press_enabled) {
                dispatch_key_event(ctx->mapped_key, KEY_EVENT_PRESS, current_tick);
            }
        }
        break;

    case KEY_STATE_HELD:
        // 保持状态，等待长按判断
        if (ctx->raw_key_value == 0) {
            // 按键释放，生成短按事件
            ctx->state = KEY_STATE_IDLE;

            if (long_press_enabled) {
                dispatch_key_event(ctx->mapped_key, KEY_EVENT_PRESS, current_tick);
            }
        } else if (long_press_enabled &&
                   (current_tick - ctx->press_start_time >= key_config.long_press_time)) {
            // 长按时间到达，进入长按状态
            ctx->state = KEY_STATE_LONG_PRESSED;
            ctx->last_repeat_time = current_tick;

            // 生成长按事件
            dispatch_key_event(ctx->mapped_key, KEY_EVENT_LONG_PRESS, current_tick);
        }
        break;

    case KEY_STATE_LONG_PRESSED:
        // 长按状态，处理重复触发
        if (ctx->raw_key_value == 0) {
            // 按键释放，回到空闲状态
            ctx->state = KEY_STATE_IDLE;
        } else if (repeat_enabled) {
            // 检查是否需要重复触发
            uint32_t repeat_start_time = ctx->press_start_time + key_config.repeat_delay;

            if (current_tick >= repeat_start_time) {
                uint32_t time_since_last_repeat = current_tick - ctx->last_repeat_time;

                if (time_since_last_repeat >= key_config.repeat_interval) {
                    // 生成重复事件
                    dispatch_key_event(ctx->mapped_key, KEY_EVENT_REPEAT, current_tick);
                    ctx->last_repeat_time = current_tick;
                }
            }
        }
        break;

    case KEY_STATE_RELEASED:
        // 释放状态（暂未使用）
        ctx->state = KEY_STATE_IDLE;
        break;

    default:
        ctx->state = KEY_STATE_IDLE;
        break;
    }
}

/**
 * @brief 更新按键上下文
 * @param raw_key 原始按键值
 * @param current_tick 当前系统滴答
 */
static void update_key_contexts(uint8_t raw_key, uint32_t current_tick) {
    for (int i = 0; i < 6; i++) {
        KeyContext* ctx = &key_contexts[i];

        // 检查该按键是否被按下
        bool is_pressed = (raw_key == raw_key_table[i]);

        // 更新原始按键值
        if (is_pressed) {
            ctx->raw_key_value = raw_key_table[i];
        } else {
            ctx->raw_key_value = 0;
        }

        // 更新映射后的功能键
        ctx->mapped_key = key_mapping[i];

        // 处理状态机
        process_key_state_machine(ctx, current_tick);
    }
}

// ============================================================================
// API函数实现
// ============================================================================

/**
 * @brief 初始化按键控制系统
 */
void key_control_init(void) {
    init_key_contexts();
    init_event_queue();

    // 默认使能长按和重复
    long_press_enabled = true;
    repeat_enabled = true;
}

/**
 * @brief 设置按键配置
 * @param config 新的按键配置
 */
void key_control_set_config(const KeyConfig* config) {
    if (config != NULL) {
        key_config = *config;
    }
}

/**
 * @brief 设置按键映射
 * @param mapping 新的按键映射数组（6个元素）
 */
void key_control_set_key_mapping(const FunKeyMapping* mapping) {
    if (mapping != NULL) {
        for (int i = 0; i < 6; i++) {
            key_mapping[i] = mapping[i];
        }
    }
}

/**
 * @brief 按键扫描函数，应该在主循环中定期调用
 * @param current_tick 当前系统滴答
 */
void key_control_scan(uint32_t current_tick) {
    // 获取原始按键值
    uint8_t raw_key = hid_get_funkey();

    // 更新按键上下文
    update_key_contexts(raw_key, current_tick);
}

/**
 * @brief 获取按键事件
 * @param event 接收事件的指针
 * @return true 获取到事件，false 无事件
 */
bool key_control_get_event(KeyEvent* event) {
    return event_queue_pop(event);
}

/**
 * @brief 检查指定按键是否被按下
 * @param key 要检查的功能键
 * @return true 按键被按下，false 按键未按下
 */
bool key_control_is_key_pressed(FunKeyMapping key) {
    // 查找对应的按键上下文
    for (int i = 0; i < 6; i++) {
        if (key_mapping[i] == key) {
            KeyContext* ctx = &key_contexts[i];
            return ctx->state == KEY_STATE_HELD || ctx->state == KEY_STATE_LONG_PRESSED;
        }
    }
    return false;
}

/**
 * @brief 获取按键按下时间（毫秒）
 * @param key 要检查的功能键
 * @return 按键按下时间，0表示未按下
 */
uint32_t key_control_get_key_press_time(FunKeyMapping key) {
    uint32_t current_tick = bsp_get_systick();

    // 查找对应的按键上下文
    for (int i = 0; i < 6; i++) {
        if (key_mapping[i] == key) {
            KeyContext* ctx = &key_contexts[i];
            if (ctx->press_start_time > 0 &&
                (ctx->state == KEY_STATE_HELD || ctx->state == KEY_STATE_LONG_PRESSED)) {
                return current_tick - ctx->press_start_time;
            }
            break;
        }
    }
    return 0;
}

/**
 * @brief 将原始按键值映射为标准功能键
 * @param raw_key 原始按键值
 * @return 映射后的功能键
 */
FunKeyMapping key_map_raw_to_function(uint8_t raw_key) {
    int8_t index = get_raw_key_index(raw_key);
    if (index >= 0 && index < 6) {
        return key_mapping[index];
    }
    return FUNKEY_NONE;
}

/**
 * @brief 将事件类型转换为字符串
 * @param type 事件类型
 * @return 事件类型字符串
 */
const char* key_event_type_to_string(KeyEventType type) {
    switch (type) {
    case KEY_EVENT_NONE:
        return "NONE";
    case KEY_EVENT_PRESS:
        return "PRESS";
    case KEY_EVENT_LONG_PRESS:
        return "LONG_PRESS";
    case KEY_EVENT_REPEAT:
        return "REPEAT";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 将功能键转换为字符串
 * @param key 功能键
 * @return 功能键字符串
 */
const char* key_mapping_to_string(FunKeyMapping key) {
    switch (key) {
    case FUNKEY_NONE:
        return "NONE";
    case FUNKEY_UP:
        return "UP";
    case FUNKEY_DOWN:
        return "DOWN";
    case FUNKEY_LEFT:
        return "LEFT";
    case FUNKEY_RIGHT:
        return "RIGHT";
    case FUNKEY_ENTER:
        return "ENTER";
    case FUNKEY_ESC:
        return "ESC";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 设置菜单系统回调函数
 * @param callback 保留参数，当前统一走事件队列分发，不再直接回调
 */
void key_control_set_menu_callback(void (*callback)(FunKeyMapping, KeyEventType)) {
    (void)callback;
}

/**
 * @brief 启用或禁用长按功能
 * @param enable true启用，false禁用
 */
void key_control_enable_long_press(bool enable) {
    long_press_enabled = enable;
}

/**
 * @brief 启用或禁用重复触发功能
 * @param enable true启用，false禁用
 */
void key_control_enable_repeat(bool enable) {
    repeat_enabled = enable;
}

/**
 * @brief 设置自动重复触发间隔
 * @param interval 重复触发间隔（毫秒）
 */
void key_control_set_auto_repeat_interval(uint32_t interval) {
    key_config.repeat_interval = interval;
}

// ============================================================================
// 与现有菜单系统的兼容性接口
// ============================================================================

/**
 * @brief 简单按键扫描接口（与现有代码兼容）
 * 这个函数可以在1ms定时中断中调用，提供基本的按键扫描功能
 */
void key_control_scan_simple(void) {
    static uint32_t last_scan_tick = 0;
    uint32_t current_tick = bsp_get_systick();

    // 每1ms扫描一次
    if (current_tick - last_scan_tick >= 1) {
        key_control_scan(current_tick);
        last_scan_tick = current_tick;
    }
}

/**
 * @brief 获取当前有效的按键事件（简化接口）
 * @return 按键事件，如果没有事件则type为KEY_EVENT_NONE
 */
KeyEvent key_control_get_current_event(void) {
    KeyEvent event = {KEY_EVENT_NONE, FUNKEY_NONE, 0};
    key_control_get_event(&event);
    return event;
}
