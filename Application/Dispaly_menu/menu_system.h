// menu_system.h
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "key_control.h"
#include "language_resources.h"
#include "user_mb_controller.h"

#pragma anon_unions

// ============================================================================
// 数据结构与类型定义
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

typedef enum {
    MENU_SPECIAL_VIEW_NONE = 0,
    MENU_SPECIAL_VIEW_IO_MONITOR,
    MENU_SPECIAL_VIEW_ALARM_LOG
} MenuSpecialView;

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

// 菜单项定义
typedef struct MenuItem {
    MenuItemTypes type;        // 菜单项类型
    uint16_t id;               // 菜单项唯一ID
    StringID text_id;          // 显示文本ID（从语言资源获取）
    StringID help_id;          // 帮助文本ID（可选）
    uint16_t icon_id;          // 图标ID（可选）
    uint16_t parent_id;        // 父菜单ID
    uint16_t first_child_id;   // 第一个子项ID（用于SUBMENU）
    uint16_t next_sibling_id;  // 下一个兄弟项ID
    uint16_t rs485_addr;       // 绑定的RS485地址，0xFFFF表示未绑定
    MenuItemType rs485_type;   // RS485数据类型
    uint8_t rs485_width;       // 写入宽度：1/2/4字节

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

#define MENU_RS485_ADDR_NONE 0xFFFFU

typedef struct {
    StringID text_id;
    uint16_t rs485_addr;
} IOMonitorPoint;

typedef struct {
    const IOMonitorPoint* x_points;
    uint8_t x_count;
    const IOMonitorPoint* y_points;
    uint8_t y_count;
} IOMonitorConfig;

typedef struct {
    uint16_t menu_id;
    uint16_t rs485_addr;
    MenuItemType rs485_type;
} AlarmLogConfig;

// 菜单上下文
typedef struct {
    uint16_t current_menu_id;  // 当前菜单ID
    uint16_t current_item_id;  // 当前选中项ID
    uint16_t scroll_position;  // 滚动位置（显示起始项索引）
    uint8_t visible_items;     // 可视项数量（根据屏幕计算）

    // 翻页相关（新增）
    uint8_t current_page;           // 当前页码（1-based）
    uint8_t total_pages;            // 总页数
    uint8_t items_in_current_page;  // 当前页实际包含的项数（动态计算）
    uint16_t page_start_index;      // 当前页第一个项的全局索引

    // 编辑状态
    bool is_editing;                // 是否处于编辑模式
    uint16_t edit_item_id;          // 正在编辑的项ID
    int32_t edit_original_value;    // 进入编辑时的原始值
    int32_t edit_pending_value;     // 编辑中的临时值，仅确认时写回
    uint8_t edit_value_step_index;  // VALUE编辑当前步长档位
    uint32_t edit_flash_tick;       // 编辑态闪烁基准时间
    bool edit_flash_inverse;        // 当前是否以反色模式显示编辑值

    // 历史记录栈（支持后退）
    uint16_t history_stack[8];       // 历史菜单栈
    uint16_t history_item_stack[8];  // 进入子菜单前的选中项栈
    uint8_t history_top;             // 栈顶指针

    // 显示缓存
    bool need_redraw;           // 需要重绘标志
    uint32_t last_redraw_time;  // 上次重绘时间

    // 当前状态
    MenuState current_state;  // 当前菜单状态

    // 特殊界面状态
    MenuSpecialView special_view;
    uint8_t io_current_page;  // 0-based
    uint8_t io_total_pages;
    uint8_t io_selected_index;  // 当前页内索引
    bool io_is_editing;
    uint32_t io_edit_flash_tick;
    bool io_edit_flash_inverse;
} MenuContext;

// 事件处理函数类型
typedef void (*EventHandler)(MenuContext* ctx, KeyEvent* event);

// 渲染回调函数类型
typedef void (*RenderCallback)(MenuItem* item, uint8_t index, bool is_selected, bool is_editing);

// 确认对话框回调函数类型
typedef void (*ConfirmCallback)(void);

// ============================================================================
// API函数声明
// ============================================================================

// 初始化接口
void menu_system_init(void);
bool menu_load_config(const MenuItem* items, uint16_t item_count);
void menu_start(uint16_t initial_menu_id);
void menu_set_language(Language lang);
bool menu_configure_io_monitor(const IOMonitorConfig* config);
bool menu_configure_alarm_log(const AlarmLogConfig* config);
void menu_open_io_monitor(void);
void menu_open_alarm_log(void);

// 导航接口
bool menu_navigate_to(uint16_t menu_id);
bool menu_navigate_back(void);
bool menu_navigate_to_root(void);
uint16_t menu_get_current_menu_id(void);
uint16_t menu_get_selected_item_id(void);
MenuState menu_get_current_state(void);

// 输入处理接口
void menu_handle_key_event(void);
void menu_handle_timer(uint32_t current_tick);
void menu_handle_data_updates(const uint16_t* changed_addrs, const uint32_t* changed_values,
                              uint8_t changed_count);

// 显示接口
void menu_request_redraw(void);
bool menu_needs_redraw(void);
void menu_force_redraw(void);
void menu_set_custom_render_callback(RenderCallback callback);

// 工具接口
void menu_show_message(StringID title_id, StringID message_id, uint32_t timeout_ms);
void menu_show_confirm(StringID title_id, StringID message_id, ConfirmCallback confirm_cb,
                       ConfirmCallback cancel_cb);
void menu_show_loading(StringID message_id);
void menu_hide_loading(void);

// 布局计算辅助函数接口
uint16_t menu_calculate_text_width(const char* text);
uint8_t menu_wrap_text_lines(char line1[32], char line2[32], const char* text, uint16_t max_width);
void menu_calculate_text_layout(TextLayout* layout, const char* text, uint16_t max_width);
bool menu_needs_wrapping(const char* text, uint16_t max_width);

// 获取当前布局配置
const ScreenLayout* menu_get_default_layout(void);

// 按键与菜单系统集成接口
void key_menu_loop(uint32_t current_tick);

// 测试接口
void test_menu_navigation(void);

#endif  // MENU_SYSTEM_H
