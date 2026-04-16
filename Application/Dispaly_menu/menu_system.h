// menu_system.h
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "language_resources.h"

#pragma anon_unions

// ============================================================================
// 数据类型定义
// ============================================================================

// 菜单项类型枚举
typedef enum {
    MENU_ITEM_TYPE_NONE = 0,  // 空项
    MENU_ITEM_TYPE_SUBMENU,   // 子菜单
    MENU_ITEM_TYPE_ACTION,    // 执行动作
    MENU_ITEM_TYPE_TOGGLE,    // 开关项
    MENU_ITEM_TYPE_VALUE,     // 数值参数
    MENU_ITEM_TYPE_STRING,    // 字符串参数
    MENU_ITEM_TYPE_LIST,      // 列表选择
    MENU_ITEM_TYPE_INFO,      // 只读信息
    MENU_ITEM_TYPE_SEPARATOR  // 分隔线
} MenuItemTypes;

// 功能键映射枚举
typedef enum {
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
    KEY_EVENT_PRESS,       // 短按
    KEY_EVENT_LONG_PRESS,  // 长按
    KEY_EVENT_REPEAT       // 重复按键（长按后）
} KeyEventType;

// 菜单状态枚举
typedef enum {
    MENU_STATE_IDLE,            // 空闲状态
    MENU_STATE_BROWSING,        // 浏览菜单
    MENU_STATE_EDITING,         // 编辑参数
    MENU_STATE_CONFIRM_DIALOG,  // 确认对话框
    MENU_STATE_MESSAGE_BOX,     // 消息框
    MENU_STATE_LOADING,         // 加载中
    MENU_STATE_ERROR            // 错误状态
} MenuState;

// 屏幕布局区域
typedef struct {
    uint8_t header_height;       // 标题栏高度
    uint8_t menu_area_top;       // 菜单区域顶部
    uint8_t menu_area_height;    // 菜单区域高度
    uint8_t footer_height;       // 底部状态栏高度
    uint8_t item_height;         // 菜单项高度
    uint8_t font_size;           // 字体大小
    uint8_t cn_char_width;       // 中文字符宽度（像素）
    uint8_t cn_char_height;      // 中文字符高度（像素）
    uint8_t en_char_width;       // 英文字符宽度（像素）
    uint8_t en_char_height;      // 英文字符高度（像素）
    uint8_t icon_size;           // 图标大小
    uint8_t padding;             // 内边距
    uint8_t max_text_width;      // 最大文本区域宽度（像素）
    uint8_t max_value_width;     // 最大值区域宽度（像素）
    uint8_t line_height;         // 行高16像素
    uint8_t max_lines_per_item;  // 每个菜单项最多显示2行
} ScreenLayout;

// 文本布局结构，用于存储换行后的文本
typedef struct {
    char line1[32];        // 第一行文本
    char line2[32];        // 第二行文本（如果有）
    uint8_t line_count;    // 实际行数（1或2）
    uint8_t total_height;  // 总高度（像素）
} TextLayout;

// 菜单项数据结构
typedef struct MenuItem {
    MenuItemTypes type;        // 菜单项类型
    uint16_t id;               // 菜单项唯一ID
    StringID text_id;          // 显示文本ID（从语言资源获取）
    StringID help_id;          // 帮助文本ID（可选）
    uint16_t icon_id;          // 图标ID（可选）
    uint16_t parent_id;        // 父菜单ID
    uint16_t first_child_id;   // 第一个子项ID（用于SUBMENU）
    uint16_t next_sibling_id;  // 下一个兄弟项ID

    union {
        // ACTION类型
        struct {
            void (*action_callback)(void);  // 动作回调函数
        };

        // TOGGLE类型
        struct {
            bool* toggle_value;            // 开关值指针
            void (*toggle_changed)(bool);  // 值改变回调
        };

        // VALUE类型
        struct {
            int32_t* value_ptr;              // 数值指针
            int32_t min_value;               // 最小值
            int32_t max_value;               // 最大值
            int32_t step_value;              // 步进值
            void (*value_changed)(int32_t);  // 值改变回调
        };

        // LIST类型
        struct {
            uint8_t* selection_ptr;              // 选择索引指针
            StringID* option_ids;                // 选项字符串ID列表
            uint8_t option_count;                // 选项数量
            void (*selection_changed)(uint8_t);  // 选择改变回调
        };

        // SUBMENU类型
        struct {
            uint16_t target_menu_id;  // 目标菜单ID
        };
    } data;
} MenuItem;

// 按键事件结构
typedef struct {
    KeyEventType type;   // 事件类型
    FunKeyMapping key;   // 按键
    uint32_t timestamp;  // 时间戳
} InputEvent;

// 菜单上下文
typedef struct {
    uint16_t current_menu_id;  // 当前菜单ID
    uint16_t current_item_id;  // 当前选中项ID
    uint16_t scroll_position;  // 滚动位置（显示起始项索引）
    uint8_t visible_items;     // 可视项数量（根据屏幕计算）

    // 编辑状态
    bool is_editing;        // 是否处于编辑模式
    uint16_t edit_item_id;  // 正在编辑的项ID

    // 历史记录栈（支持后退）
    uint16_t history_stack[8];  // 历史菜单栈
    uint8_t history_top;        // 栈顶指针

    // 显示缓存
    bool need_redraw;           // 需要重绘标志
    uint32_t last_redraw_time;  // 上次重绘时间

    // 当前状态
    MenuState current_state;  // 当前菜单状态
} MenuContext;

// 按键配置
typedef struct {
    uint32_t debounce_time;    // 消抖时间（ms）
    uint32_t long_press_time;  // 长按时间（ms）
    uint32_t repeat_delay;     // 重复触发延迟（ms）
    uint32_t repeat_interval;  // 重复触发间隔（ms）
} KeyConfig;

// 事件处理函数类型
typedef void (*EventHandler)(MenuContext* ctx, InputEvent* event);

// 渲染回调函数类型
typedef void (*RenderCallback)(MenuItem* item, uint8_t index, bool is_selected, bool is_editing);

// 确认对话框回调函数类型
typedef void (*ConfirmCallback)(void);

// ============================================================================
// API函数声明
// ============================================================================

// 初始化API
void menu_system_init(void);
bool menu_load_config(const MenuItem* items, uint16_t item_count);
void menu_start(uint16_t initial_menu_id);
void menu_set_language(Language lang);

// 导航API
bool menu_navigate_to(uint16_t menu_id);
bool menu_navigate_back(void);
bool menu_navigate_to_root(void);
uint16_t menu_get_current_menu_id(void);
uint16_t menu_get_selected_item_id(void);
MenuState menu_get_current_state(void);

// 输入处理API
void menu_handle_key_event(FunKeyMapping key, KeyEventType event_type);
void menu_handle_timer(uint32_t current_tick);
void menu_set_key_config(const KeyConfig* config);

// 显示API
void menu_request_redraw(void);
bool menu_needs_redraw(void);
void menu_force_redraw(void);
void menu_set_custom_render_callback(RenderCallback callback);

// 工具API
void menu_show_message(StringID title_id, StringID message_id, uint32_t timeout_ms);
void menu_show_confirm(StringID title_id, StringID message_id, ConfirmCallback confirm_cb, ConfirmCallback cancel_cb);
void menu_show_loading(StringID message_id);
void menu_hide_loading(void);

// 按键映射API
void menu_set_key_mapping(const FunKeyMapping* mapping);

// 布局计算辅助函数
uint16_t menu_calculate_text_width(const char* text);
uint8_t menu_wrap_text_lines(char line1[32], char line2[32], const char* text, uint16_t max_width);
void menu_calculate_text_layout(TextLayout* layout, const char* text, uint16_t max_width);
bool menu_needs_wrapping(const char* text, uint16_t max_width);

// 获取当前布局配置
const ScreenLayout* menu_get_default_layout(void);

void test_menu_navigation(void);

#endif  // MENU_SYSTEM_H
