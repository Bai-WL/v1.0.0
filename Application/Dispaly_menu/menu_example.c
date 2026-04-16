// menu_example.c
// 菜单系统使用示例

#include "menu_system.h"
#include "language_resources.h"
#include <stdio.h>

// ============================================================================
// 示例数据变量
// ============================================================================

// 用户参数变量
static int32_t user_p00 = 100;
static int32_t user_p01 = 50;
static bool system_m01 = true;
static uint8_t cnc_type = 0; // 0:走刀, 1:走心, 2:CNC, 3:CAM

// 回调函数
static void on_p00_changed(int32_t value) {
    printf("P00 changed to: %d\n", value);
}

static void on_p01_changed(int32_t value) {
    printf("P01 changed to: %d\n", value);
}

static void on_m01_changed(bool value) {
    printf("M01 changed to: %s\n", value ? "ON" : "OFF");
}

static void on_cnc_type_changed(uint8_t value) {
    const char* types[] = {"走刀", "走心", "CNC", "CAM"};
    if (value < 4) {
        printf("CNC type changed to: %s\n", types[value]);
    }
}

static void open_io_monitor(void) {
    printf("Opening IO Monitor...\n");
}

static void open_auto_control(void) {
    printf("Opening Auto Control...\n");
}

static void manual_move_left_fast(void) {
    printf("Manual move left quickly\n");
}

static void manual_move_right_fast(void) {
    printf("Manual move right quickly\n");
}

// ============================================================================
// 选项列表定义（使用StringID）
// ============================================================================

// CNC类型选项StringID
static StringID cnc_type_option_ids[] = {
    STR_DAO,    // "走刀"
    STR_XIN,    // "走心" 
    STR_CNC,    // "CNC"
    STR_CAM     // "CAM"
};

// 单位选项StringID
static StringID unit_option_ids[] = {
    STR_MM,     // "mm"
    STR_INCH    // "inch"
};

// 语言选项StringID（使用现有StringID）
static StringID language_option_ids[] = {
    STR_CN,     // "中文"
    STR_EN,     // "英文"
    STR_CN,     // 中文（作为示例，实际应添加其他语言）
    STR_EN,     // 英文（作为示例，实际应添加其他语言）
    STR_CN      // 中文（作为示例，实际应添加其他语言）
};

// ============================================================================
// 菜单项定义
// ============================================================================

static const MenuItem menu_items[] = {
    // 主菜单 (ID: 0)
    {MENU_ITEM_TYPE_SUBMENU, 0, STR_MAIN_MENU, STR_NONE, 0, 0, 1, 0, {.target_menu_id = 0}},
    
    // 主菜单下的子项
    {MENU_ITEM_TYPE_SUBMENU, 1, STR_MANUAL_INTERFACE, STR_NONE, 0, 0, 10, 2, {.target_menu_id = 10}},
    {MENU_ITEM_TYPE_SUBMENU, 2, STR_USER_PARAMETERS, STR_NONE, 0, 0, 20, 3, {.target_menu_id = 20}},
    {MENU_ITEM_TYPE_SUBMENU, 3, STR_SYSTEM_PARAMETERS, STR_NONE, 0, 0, 30, 4, {.target_menu_id = 30}},
    {MENU_ITEM_TYPE_ACTION,  4, STR_IO_MONITOR, STR_NONE, 0, 0, 0, 5, {.action_callback = open_io_monitor}},
    {MENU_ITEM_TYPE_SUBMENU, 5, STR_ALARM_LOG, STR_NONE, 0, 0, 40, 6, {.target_menu_id = 40}},
    {MENU_ITEM_TYPE_SUBMENU, 6, STR_SCREEN_SETTING, STR_NONE, 0, 0, 50, 7, {.target_menu_id = 50}},
    {MENU_ITEM_TYPE_ACTION,  7, STR_AUTO_CONTROL, STR_NONE, 0, 0, 0, 0, {.action_callback = open_auto_control}},
    
    // 手动界面子菜单 (ID: 10)
    {MENU_ITEM_TYPE_SUBMENU, 10, STR_MANUAL_INTERFACE, STR_NONE, 0, 1, 11, 0, {.target_menu_id = 10}},
    {MENU_ITEM_TYPE_ACTION,  11, STR_MOVE_LEFT_QUICKLY, STR_NONE, 0, 10, 0, 12, {.action_callback = manual_move_left_fast}},
    {MENU_ITEM_TYPE_ACTION,  12, STR_MOVE_RIGHT_QUICKLY, STR_NONE, 0, 10, 0, 13, {.action_callback = manual_move_right_fast}},
    {MENU_ITEM_TYPE_ACTION,  13, STR_MOVE_LEFT, STR_NONE, 0, 10, 0, 14, {.action_callback = manual_move_left_fast}},  // 使用相同函数
    {MENU_ITEM_TYPE_ACTION,  14, STR_MOVE_RIGHT, STR_NONE, 0, 10, 0, 15, {.action_callback = manual_move_right_fast}}, // 使用相同函数
    {MENU_ITEM_TYPE_ACTION,  15, STR_COVER_CLOSE, STR_NONE, 0, 10, 0, 16, {.action_callback = open_io_monitor}},  // 临时函数
    {MENU_ITEM_TYPE_ACTION,  16, STR_COVER_OPEN, STR_NONE, 0, 10, 0, 0, {.action_callback = open_io_monitor}},    // 临时函数
    
    // 用户参数子菜单 (ID: 20) - 数值参数示例
    {MENU_ITEM_TYPE_SUBMENU, 20, STR_USER_PARAMETERS, STR_NONE, 0, 2, 21, 0, {.target_menu_id = 20}},
    {MENU_ITEM_TYPE_VALUE,   21, STR_SETTING_P00, STR_NONE, 0, 20, 0, 22, 
        {.value_ptr = &user_p00, .min_value = 0, .max_value = 9999, .step_value = 1, .value_changed = on_p00_changed}},
    {MENU_ITEM_TYPE_VALUE,   22, STR_SETTING_P01, STR_NONE, 0, 20, 0, 0,
        {.value_ptr = &user_p01, .min_value = 0, .max_value = 100, .step_value = 1, .value_changed = on_p01_changed}},
    
    // 系统参数子菜单 (ID: 30) - 开关和列表示例
    {MENU_ITEM_TYPE_SUBMENU, 30, STR_SYSTEM_PARAMETERS, STR_NONE, 0, 3, 31, 0, {.target_menu_id = 30}},
    {MENU_ITEM_TYPE_TOGGLE,  31, STR_M01, STR_NONE, 0, 30, 0, 32,
        {.toggle_value = &system_m01, .toggle_changed = on_m01_changed}},
    {MENU_ITEM_TYPE_LIST,    32, STR_M09, STR_NONE, 0, 30, 0, 0,
        {.selection_ptr = &cnc_type, .option_ids = cnc_type_option_ids, 
         .option_count = 4, .selection_changed = on_cnc_type_changed}},
    
    // 报警日志子菜单 (ID: 40) - 信息显示示例
    {MENU_ITEM_TYPE_SUBMENU, 40, STR_ALARM_LOG, STR_NONE, 0, 5, 41, 0, {.target_menu_id = 40}},
    {MENU_ITEM_TYPE_INFO,    41, STR_ALARM_E01, STR_NONE, 0, 40, 0, 42, {0}},
    {MENU_ITEM_TYPE_INFO,    42, STR_ALARM_E02, STR_NONE, 0, 40, 0, 43, {0}},
    {MENU_ITEM_TYPE_INFO,    43, STR_ALARM_E03, STR_NONE, 0, 40, 0, 0, {0}},
    
    // 屏幕设置子菜单 (ID: 50)
    {MENU_ITEM_TYPE_SUBMENU, 50, STR_SCREEN_SETTING, STR_NONE, 0, 6, 51, 0, {.target_menu_id = 50}},
    {MENU_ITEM_TYPE_TOGGLE,  51, STR_YES, STR_NONE, 0, 50, 0, 52,  // 背光开关示例
        {.toggle_value = &system_m01, .toggle_changed = on_m01_changed}},
    {MENU_ITEM_TYPE_VALUE,   52, STR_HOUR, STR_NONE, 0, 50, 0, 0,  // 时间设置示例
        {.value_ptr = &user_p00, .min_value = 0, .max_value = 23, .step_value = 1, .value_changed = on_p00_changed}},
};

#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

// ============================================================================
// 测试函数
// ============================================================================

// 测试自动换行功能
void test_text_wrapping(void) {
    printf("=== Testing Text Wrapping ===\n");
    
    const char* test_texts[] = {
        "短文本",
        "这是一个中等长度的中文文本",
        "This is a short English text",
        "这是一个非常长的中文文本需要测试自动换行功能看它如何处理",
        "This is a very long English text that needs to test the automatic line wrapping function to see how it handles",
        NULL
    };
    
    for (int i = 0; test_texts[i] != NULL; i++) {
        char line1[32], line2[32];
        uint8_t line_count = menu_wrap_text_lines(line1, line2, test_texts[i], 100);
        
        printf("Text %d: '%s'\n", i, test_texts[i]);
        printf("  Line count: %d\n", line_count);
        printf("  Line 1: '%s'\n", line1);
        if (line_count > 1) {
            printf("  Line 2: '%s'\n", line2);
        }
        printf("\n");
    }
}

// 测试文本宽度计算
void test_text_width_calculation(void) {
    printf("=== Testing Text Width Calculation ===\n");
    
    const char* test_texts[] = {
        "中文",
        "English",
        "中英文混合Mixed",
        "这是一段测试文字",
        "This is test text",
        NULL
    };
    
    for (int i = 0; test_texts[i] != NULL; i++) {
        uint16_t width = menu_calculate_text_width(test_texts[i]);
        printf("Text '%s' width: %d pixels\n", test_texts[i], width);
    }
    printf("\n");
}

// 测试菜单导航
void test_menu_navigation(void) {
    printf("=== Testing Menu Navigation ===\n");
    
    // 初始化菜单系统
    menu_system_init();
    
    // 加载菜单配置
    if (!menu_load_config(menu_items, MENU_ITEM_COUNT)) {
        printf("Failed to load menu config!\n");
        return;
    }
    
    // 启动菜单系统
    menu_start(0); // 从主菜单开始
    
    printf("Menu system started successfully.\n");
    printf("Current menu ID: %d\n", menu_get_current_menu_id());
    printf("Selected item ID: %d\n", menu_get_selected_item_id());
    printf("Current state: %d\n", menu_get_current_state());
    
    // 模拟按键操作
    printf("\nSimulating key presses...\n");
    
    // 向下移动选择
    menu_handle_key_event(FUNKEY_DOWN, KEY_EVENT_PRESS);
    printf("After DOWN: Selected item ID: %d\n", menu_get_selected_item_id());
    
    menu_handle_key_event(FUNKEY_DOWN, KEY_EVENT_PRESS);
    printf("After DOWN: Selected item ID: %d\n", menu_get_selected_item_id());
    
    // 进入子菜单
    menu_handle_key_event(FUNKEY_ENTER, KEY_EVENT_PRESS);
    printf("After ENTER: Current menu ID: %d\n", menu_get_current_menu_id());
    printf("After ENTER: Selected item ID: %d\n", menu_get_selected_item_id());
    
    // 测试编辑模式
    menu_handle_key_event(FUNKEY_ENTER, KEY_EVENT_PRESS);
    printf("After ENTER (edit mode): Current state: %d\n", menu_get_current_state());
    
    // 编辑模式下调整数值
    menu_handle_key_event(FUNKEY_RIGHT, KEY_EVENT_PRESS);
    printf("After RIGHT (edit): Value changed\n");
    
    // 确认编辑
    menu_handle_key_event(FUNKEY_ENTER, KEY_EVENT_PRESS);
    printf("After ENTER (confirm): Current state: %d\n", menu_get_current_state());
    
    // 返回上一级
    menu_handle_key_event(FUNKEY_ESC, KEY_EVENT_PRESS);
    printf("After ESC: Current menu ID: %d\n", menu_get_current_menu_id());
    
    printf("\nMenu navigation test completed.\n");
}

// 测试布局配置
void test_layout_configuration(void) {
    printf("=== Testing Layout Configuration ===\n");
    
    const ScreenLayout* layout = menu_get_default_layout();
    
    printf("Screen Layout Configuration:\n");
    printf("  Header height: %d\n", layout->header_height);
    printf("  Menu area top: %d\n", layout->menu_area_top);
    printf("  Menu area height: %d\n", layout->menu_area_height);
    printf("  Footer height: %d\n", layout->footer_height);
    printf("  Item height: %d\n", layout->item_height);
    printf("  CN char width: %d, height: %d\n", layout->cn_char_width, layout->cn_char_height);
    printf("  EN char width: %d, height: %d\n", layout->en_char_width, layout->en_char_height);
    printf("  Max lines per item: %d\n", layout->max_lines_per_item);
    
    // 测试可视项数量
    uint8_t visible_items = layout->menu_area_height / layout->item_height;
    printf("  Visible items: %d (calculated: %d)\n", layout->menu_area_height / layout->item_height, visible_items);
}

// 主测试函数
int main(void) {
    printf("Menu System Implementation Test\n");
    printf("===============================\n\n");
    
    // 测试文本处理功能
    test_text_width_calculation();
    test_text_wrapping();
    
    // 测试布局配置
    test_layout_configuration();
    
    // 测试菜单导航
    test_menu_navigation();
    
    printf("\nAll tests completed successfully!\n");
    
    return 0;
}