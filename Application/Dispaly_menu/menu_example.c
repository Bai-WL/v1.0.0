// menu_example.c
// 菜单系统配置示例

#include <stdio.h>

#include "language_resources.h"
#include "menu_system.h"

// ============================================================================
// 示例数据
// ============================================================================

// 用户参数变量
static int32_t user_p00 = 100;
static int32_t user_p01 = 50;
static bool system_m01 = true;
static uint8_t cnc_type = 0;  // 0:走刀, 1:走心, 2:CNC, 3:CAM

// 回调函数
static void on_p00_changed(int32_t value) {}

static void on_p01_changed(int32_t value) {}

static void on_m01_changed(bool value) {}

static void on_cnc_type_changed(uint8_t value) {}

// IO监控地址集中配置，后续可直接按PLC地址表替换。
#define IO_MONITOR_X_BASE_ADDR 0U
#define IO_MONITOR_Y_BASE_ADDR 100U

static const IOMonitorPoint io_monitor_x_points[] = {
    {STR_X0, IO_MONITOR_X_BASE_ADDR + 0U},   {STR_X1, IO_MONITOR_X_BASE_ADDR + 1U},
    {STR_X2, IO_MONITOR_X_BASE_ADDR + 2U},   {STR_X3, IO_MONITOR_X_BASE_ADDR + 3U},
    {STR_X4, IO_MONITOR_X_BASE_ADDR + 4U},   {STR_X5, IO_MONITOR_X_BASE_ADDR + 5U},
    {STR_X6, IO_MONITOR_X_BASE_ADDR + 6U},   {STR_X7, IO_MONITOR_X_BASE_ADDR + 7U},
    {STR_X8, IO_MONITOR_X_BASE_ADDR + 8U},   {STR_X9, IO_MONITOR_X_BASE_ADDR + 9U},
    {STR_X10, IO_MONITOR_X_BASE_ADDR + 10U}, {STR_X11, IO_MONITOR_X_BASE_ADDR + 11U},
    {STR_X12, IO_MONITOR_X_BASE_ADDR + 12U}, {STR_X13, IO_MONITOR_X_BASE_ADDR + 13U},
    {STR_X14, IO_MONITOR_X_BASE_ADDR + 14U}, {STR_X15, IO_MONITOR_X_BASE_ADDR + 15U},
    {STR_X16, IO_MONITOR_X_BASE_ADDR + 16U},
};

static const IOMonitorPoint io_monitor_y_points[] = {
    {STR_Y0, IO_MONITOR_Y_BASE_ADDR + 0U},   {STR_Y1, IO_MONITOR_Y_BASE_ADDR + 1U},
    {STR_Y2, IO_MONITOR_Y_BASE_ADDR + 2U},   {STR_Y3, IO_MONITOR_Y_BASE_ADDR + 3U},
    {STR_Y4, IO_MONITOR_Y_BASE_ADDR + 4U},   {STR_Y5, IO_MONITOR_Y_BASE_ADDR + 5U},
    {STR_Y6, IO_MONITOR_Y_BASE_ADDR + 6U},   {STR_Y7, IO_MONITOR_Y_BASE_ADDR + 7U},
    {STR_Y8, IO_MONITOR_Y_BASE_ADDR + 8U},   {STR_Y9, IO_MONITOR_Y_BASE_ADDR + 9U},
    {STR_Y10, IO_MONITOR_Y_BASE_ADDR + 10U}, {STR_Y11, IO_MONITOR_Y_BASE_ADDR + 11U},
    {STR_Y12, IO_MONITOR_Y_BASE_ADDR + 12U}, {STR_Y13, IO_MONITOR_Y_BASE_ADDR + 13U},
    {STR_Y14, IO_MONITOR_Y_BASE_ADDR + 14U}, {STR_Y15, IO_MONITOR_Y_BASE_ADDR + 15U},
    {STR_Y16, IO_MONITOR_Y_BASE_ADDR + 16U}, {STR_Y17, IO_MONITOR_Y_BASE_ADDR + 17U},
};

static const IOMonitorConfig io_monitor_config = {
    .x_points = io_monitor_x_points,
    .x_count = (uint8_t)(sizeof(io_monitor_x_points) / sizeof(io_monitor_x_points[0])),
    .y_points = io_monitor_y_points,
    .y_count = (uint8_t)(sizeof(io_monitor_y_points) / sizeof(io_monitor_y_points[0])),
};

static const AlarmLogConfig alarm_log_config = {
    .menu_id = 40U,
    .rs485_addr = 12907U,
    .rs485_type = MENU_TYPE_VALUE_UINT16,
};

static void open_io_monitor(void) {
    menu_open_io_monitor();
}

static void open_auto_control(void) {}

static void manual_move_left_fast(void) {}

static void manual_move_right_fast(void) {}

// ============================================================================
// 选项列表定义（使用 StringID）
// ============================================================================

// CNC类型选项StringID
static StringID cnc_type_option_ids[] = {
    STR_DAO,  // "走刀"
    STR_XIN,  // "走心"
    STR_CNC,  // "CNC"
    STR_CAM   // "CAM"
};

// 单位选项StringID
static StringID unit_option_ids[] = {
    STR_MM,   // "mm"
    STR_INCH  // "inch"
};

// 语言选项StringID（使用现有StringID）
static StringID language_option_ids[] = {
    STR_CN,  // "中文"
    STR_EN,  // "英文"
    STR_CN,  // 中文（作为示例，实际应添加其他语言）
    STR_EN,  // 英文（作为示例，实际应添加其他语言）
    STR_CN   // 中文（作为示例，实际应添加其他语言）
};

// ============================================================================
// 菜单项配置
// ============================================================================
// clang-format off
static const MenuItem menu_items[] = {
    // 主菜单 (ID: 0)
    {MENU_ITEM_TYPE_SUBMENU, 0, STR_MAIN_MENU, STR_NONE, 0, 0, 1, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 0}},
    
    // 主菜单下的子项
    {MENU_ITEM_TYPE_SUBMENU, 1, STR_MANUAL_INTERFACE, STR_NONE, 0, 0, 10, 2,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 10}},
    {MENU_ITEM_TYPE_SUBMENU, 2, STR_USER_PARAMETERS, STR_NONE, 0, 0, 20, 3,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 20}},
    {MENU_ITEM_TYPE_SUBMENU, 3, STR_SYSTEM_PARAMETERS, STR_NONE, 0, 0, 30, 4,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 30}},
    {MENU_ITEM_TYPE_ACTION,  4, STR_IO_MONITOR, STR_NONE, 0, 0, 0, 5,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = open_io_monitor}},
    {MENU_ITEM_TYPE_SUBMENU, 5, STR_ALARM_LOG, STR_NONE, 0, 0, 40, 6,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 40}},
    {MENU_ITEM_TYPE_SUBMENU, 6, STR_SCREEN_SETTING, STR_NONE, 0, 0, 50, 7,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 50}},
    {MENU_ITEM_TYPE_ACTION,  7, STR_AUTO_CONTROL, STR_NONE, 0, 0, 0, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = open_auto_control}},
    
    // 手动界面子菜单 (ID: 10)
    {MENU_ITEM_TYPE_SUBMENU, 10, STR_MANUAL_INTERFACE, STR_NONE, 0, 1, 11, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 10}},
    {MENU_ITEM_TYPE_ACTION,  11, STR_MOVE_LEFT_QUICKLY, STR_NONE, 0, 10, 0, 12,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = manual_move_left_fast}},
    {MENU_ITEM_TYPE_ACTION,  12, STR_MOVE_RIGHT_QUICKLY, STR_NONE, 0, 10, 0, 13,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = manual_move_right_fast}},
    {MENU_ITEM_TYPE_ACTION,  13, STR_MOVE_LEFT, STR_NONE, 0, 10, 0, 14,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = manual_move_left_fast}},  // 使用相同函数
    {MENU_ITEM_TYPE_ACTION,  14, STR_MOVE_RIGHT, STR_NONE, 0, 10, 0, 15,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = manual_move_right_fast}}, // 使用相同函数
    {MENU_ITEM_TYPE_ACTION,  15, STR_COVER_CLOSE, STR_NONE, 0, 10, 0, 16,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = open_io_monitor}},  // 临时函数
    {MENU_ITEM_TYPE_ACTION,  16, STR_COVER_OPEN, STR_NONE, 0, 10, 0, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.action_callback = open_io_monitor}},    // 临时函数
    
    // 用户参数子菜单 (ID: 20) - 数值参数示例
    {MENU_ITEM_TYPE_SUBMENU, 20, STR_USER_PARAMETERS, STR_NONE, 0, 2, 21, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 20}},
    {MENU_ITEM_TYPE_VALUE,   21, STR_SETTING_P00, STR_NONE, 0, 20, 0, 22, 
        2914, MENU_TYPE_VALUE_UINT16, 2,
        {.value_ptr = &user_p00, .min_value = 0, .max_value = 9999, .step_value = 1, .value_changed = on_p00_changed}},
    {MENU_ITEM_TYPE_VALUE,   22, STR_SETTING_P01, STR_NONE, 0, 20, 0, 0,
        2915, MENU_TYPE_VALUE_UINT16, 2,
        {.value_ptr = &user_p01, .min_value = 0, .max_value = 100, .step_value = 1, .value_changed = on_p01_changed}},
    
    // 系统参数子菜单 (ID: 30) - 开关和列表示例
    {MENU_ITEM_TYPE_SUBMENU, 30, STR_SYSTEM_PARAMETERS, STR_NONE, 0, 3, 31, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 30}},
    {MENU_ITEM_TYPE_TOGGLE,  31, STR_M01, STR_NONE, 0, 30, 0, 32,
        1, MENU_TYPE_VALUE_BIT, 1, {.toggle_value = &system_m01, .toggle_changed = on_m01_changed}},
    {MENU_ITEM_TYPE_LIST,    32, STR_M09, STR_NONE, 0, 30, 0, 0,
        2916, MENU_TYPE_VALUE_UINT16, 2,
        {.selection_ptr = &cnc_type, .option_ids = cnc_type_option_ids, 
         .option_count = 4, .selection_changed = on_cnc_type_changed}},
    
    // 报警日志子菜单 (ID: 40) - 信息显示示例
    {MENU_ITEM_TYPE_SUBMENU, 40, STR_ALARM_LOG, STR_NONE, 0, 5, 41, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 40}},
    {MENU_ITEM_TYPE_INFO,    41, STR_ALARM_E01, STR_NONE, 0, 40, 0, 42,
        12907, MENU_TYPE_VALUE_UINT16, 2, {0}},
    {MENU_ITEM_TYPE_INFO,    42, STR_ALARM_E02, STR_NONE, 0, 40, 0, 43,
        12908, MENU_TYPE_VALUE_UINT16, 2, {0}},
    {MENU_ITEM_TYPE_INFO,    43, STR_ALARM_E03, STR_NONE, 0, 40, 0, 0,
        12909, MENU_TYPE_VALUE_UINT16, 2, {0}},
    
    // 屏幕设置子菜单 (ID: 50)
    {MENU_ITEM_TYPE_SUBMENU, 50, STR_SCREEN_SETTING, STR_NONE, 0, 6, 51, 0,
        MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, {.target_menu_id = 50}},
    {MENU_ITEM_TYPE_TOGGLE,  51, STR_YES, STR_NONE, 0, 50, 0, 52,  // 背光开关示例
        2, MENU_TYPE_VALUE_BIT, 1, {.toggle_value = &system_m01, .toggle_changed = on_m01_changed}},
    {MENU_ITEM_TYPE_VALUE,   52, STR_HOUR, STR_NONE, 0, 50, 0, 0,  // 时间设置示例
        12910, MENU_TYPE_VALUE_UINT16, 2,
        {.value_ptr = &user_p00, .min_value = 0, .max_value = 23, .step_value = 1, .value_changed = on_p00_changed}},
};
// clang-format on
#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

// 测试菜单启动流程
void test_menu_navigation(void) {
    // 初始化菜单系统
    menu_system_init();
    menu_configure_io_monitor(&io_monitor_config);
    menu_configure_alarm_log(&alarm_log_config);
    menu_load_config(menu_items, MENU_ITEM_COUNT);
    // 启动菜单系统
    menu_start(0);  // 从主菜单开始
}
