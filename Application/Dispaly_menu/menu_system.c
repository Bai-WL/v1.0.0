// menu_system.c
#include "menu_system.h"

#include <stdio.h>
#include <string.h>

#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f30x_rtc.h"
#include "device_storage.h"
#include "jlx_display_ext.h"
#include "poll_manager.h"
#include "user_poll.h"

// ============================================================================
// 内部常量定义
// ============================================================================

// 默认屏幕布局配置（基于192x128像素显示屏）
static const ScreenLayout default_layout = {
    .header_height = 16,     // 标题栏16像素
    .menu_area_top = 20,     // 菜单区域从20像素开始
    .menu_area_height = 88,  // 菜单区域高度88像素
    .footer_height = 24,     // 底部状态栏24像素
    .item_height = 16,       // 每个菜单项16像素
    .font_size = 0,          // 8号字体
    .cn_char_width = 13,     // 中文字符宽度13像素
    .cn_char_height = 16,    // 中文字符高度16像素
    .en_char_width = 7,      // 英文字符宽度7像素
    .en_char_height = 16,    // 英文字符高度16像素
    .icon_size = 12,         // 图标12x12像素
    .padding = 2,            // 内边距2像素
    .max_text_width = 100,   // 文本区域最大宽度（像素）- 根据布局计算
    .max_value_width = 50,   // 附加信息区域宽度（像素）
    .line_height = 16,       // 行高16像素
    .max_lines_per_item = 2  // 每个菜单项最多显示2行
};

static const int32_t value_edit_steps[] = {1, 10, 100, 1000, 10000};
#define VALUE_EDIT_STEP_COUNT ((uint8_t)(sizeof(value_edit_steps) / sizeof(value_edit_steps[0])))

// 计算可视项数量（按默认单行高度估算）
#define VISIBLE_ITEMS (default_layout.menu_area_height / default_layout.item_height)  // 88 / 16 = 5
#define IO_MONITOR_COLUMNS 4U
#define IO_MONITOR_ROWS 4U
#define IO_MONITOR_PAGE_SIZE (IO_MONITOR_COLUMNS * IO_MONITOR_ROWS)
#define ALARM_LOG_MAX_ENTRIES 10U
#define ALARM_LOG_TIME_X 16U
#define ALARM_LOG_TEXT_X 76U
#define ALARM_LOG_TEXT_WIDTH ((JLXLCD_W > ALARM_LOG_TEXT_X) ? (JLXLCD_W - ALARM_LOG_TEXT_X) : 0U)

typedef struct {
    bool valid;
    uint16_t alarm_value;
    StringID text_id;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool unread;
} AlarmLogEntry;

// ============================================================================
// 内部全局变量
// ============================================================================

// 菜单配置
static const MenuItem* menu_items = NULL;
static uint16_t menu_item_count = 0;
static IOMonitorConfig io_monitor_config = {0};
static AlarmLogConfig alarm_log_config = {0U, MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16};
static AlarmLogEntry alarm_log_entries[ALARM_LOG_MAX_ENTRIES] = {0};
static uint8_t alarm_log_head = 0U;
static uint8_t alarm_log_count = 0U;
static uint8_t alarm_log_current_page = 0U;
static uint8_t alarm_log_total_pages = 1U;
static uint8_t alarm_log_page_start_index = 0U;
static uint8_t alarm_log_selected_row = 0U;
static uint32_t alarm_log_last_value = 0U;
static bool alarm_log_has_last_value = false;
static bool momentary_enter_active = false;
static uint16_t momentary_active_item_id = 0U;

// 菜单上下文
static MenuContext menu_ctx = {.current_menu_id = 0,
                               .current_item_id = 0,
                               .scroll_position = 0,
                               .visible_items = 5,  // VISIBLE_ITEMS 计算结果为 88/16 = 5
                               .current_page = 1,
                               .total_pages = 1,
                               .items_in_current_page = 0,
                               .page_start_index = 0,
                               .is_editing = false,
                               .edit_item_id = 0,
                               .edit_original_value = 0,
                               .edit_pending_value = 0,
                               .edit_value_step_index = 0,
                               .edit_flash_tick = 0,
                               .edit_flash_inverse = false,
                               .history_top = 0,
                               .need_redraw = true,
                               .last_redraw_time = 0,
                               .current_state = MENU_STATE_IDLE,
                               .special_view = MENU_SPECIAL_VIEW_NONE,
                               .io_current_page = 0,
                               .io_total_pages = 0,
                               .io_selected_index = 0,
                               .io_is_editing = false,
                               .io_edit_flash_tick = 0,
                               .io_edit_flash_inverse = false};

// 自定义渲染回调
static RenderCallback custom_render_callback = NULL;

// ============================================================================
// 内部辅助函数声明
// ============================================================================

static MenuItem* get_menu_item(uint16_t id);
static MenuItem* get_first_child(uint16_t parent_id);
static MenuItem* get_next_sibling(uint16_t current_id);
static uint16_t count_children(uint16_t parent_id);
static MenuItem* get_child_at_index(uint16_t parent_id, uint16_t index);
static uint16_t get_child_index(uint16_t parent_id, uint16_t child_id);
static uint16_t get_menu_item_max_text_width(const MenuItem* item, uint8_t text_x);
static uint8_t get_menu_item_height(MenuItem* item);
static uint8_t build_page_from_index(uint16_t parent_id, uint16_t start_index,
                                     uint16_t* page_height);
static bool get_page_bounds_by_number(uint16_t parent_id, uint8_t page_number,
                                      uint16_t* page_start_index, uint8_t* items_in_page,
                                      uint8_t* total_pages);
static void push_history_snapshot(void);
static bool pop_history_snapshot(uint16_t* menu_id, uint16_t* item_id);
static void select_item_and_refresh(uint16_t item_id);
static bool enter_menu_internal(uint16_t menu_id, bool save_history);
static bool navigate_relative_page(int8_t page_offset);
static void render_menu_item_internal(uint8_t y_pos, MenuItem* item, bool is_selected,
                                      bool is_editing);
static void render_header_internal(void);
static void render_footer_internal(void);
static int32_t get_editable_item_value(const MenuItem* item);
static int32_t get_render_value(const MenuItem* item, bool is_selected, bool is_editing);
static bool is_value_editing_item(const MenuItem* item);
static uint8_t get_value_step_index_by_step(int32_t step_value);
static int32_t get_current_value_edit_step(void);
static void switch_value_edit_step(int8_t direction);
static void begin_edit_session(MenuItem* item);
static bool apply_pending_edit_value(MenuItem* item);
static void update_edit_flash_state(uint32_t current_tick);
static void handle_move_up(void);
static void handle_move_down(void);
static void handle_select_item(void);
static void handle_back(void);
static void handle_edit_increase(void);
static void handle_edit_decrease(void);
static void handle_edit_confirm(void);
static void handle_edit_cancel(void);
static bool menu_item_has_rs485_binding(const MenuItem* item);
static bool menu_item_read_rs485_value(const MenuItem* item, int32_t* value);
static void menu_item_update_shadow_value(const MenuItem* item, int32_t value);
static void menu_register_cache_for_item(const MenuItem* item);
static void menu_register_all_cache_items(void);
static void menu_register_current_page_polling(uint16_t menu_id);
static bool io_monitor_is_configured(void);
static uint8_t io_monitor_get_total_pages_internal(void);
static bool io_monitor_page_is_output(uint8_t page_index);
static uint8_t io_monitor_get_page_count(uint8_t page_index);
static const IOMonitorPoint* io_monitor_get_point(uint8_t page_index, uint8_t slot,
                                                  bool* is_output);
static void io_monitor_register_all_polling(void);
static void io_monitor_sync_paging(void);
static void io_monitor_close(void);
static void io_monitor_move_up(void);
static void io_monitor_move_down(void);
static void io_monitor_page_up(void);
static void io_monitor_page_down(void);
static void io_monitor_select(void);
static void io_monitor_back(void);
static void render_io_monitor_view(void);
static bool alarm_log_is_configured(void);
static StringID alarm_log_value_to_string_id(uint16_t alarm_value);
static uint8_t alarm_log_get_entry_height(const AlarmLogEntry* entry);
static uint8_t alarm_log_build_page(uint8_t start_index, uint16_t* page_height);
static bool alarm_log_get_page_bounds(uint8_t page_number, uint8_t* page_start_index,
                                      uint8_t* entries_in_page, uint8_t* total_pages);
static uint8_t alarm_log_get_page_entry_count(uint8_t page_index);
static AlarmLogEntry* alarm_log_get_entry_by_display_index(uint8_t display_index);
static void alarm_log_register_polling(void);
static void alarm_log_sync_paging(void);
static void alarm_log_mark_all_read(void);
static void alarm_log_append(uint16_t alarm_value);
static bool alarm_log_open(bool save_history);
static void alarm_log_close(void);
static void alarm_log_move_up(void);
static void alarm_log_move_down(void);
static void alarm_log_page_up(void);
static void alarm_log_page_down(void);
static void alarm_log_select(void);
static void alarm_log_back(void);
static void render_alarm_log_view(void);
static bool menu_item_set_momentary_state(MenuItem* item, bool on);
static void menu_update_momentary_enter_state(void);

// 按键事件内部处理函数
static void handle_short_press(KeyEvent* event);
static void handle_long_press(KeyEvent* event);
static void handle_repeat_event(KeyEvent* event);

// 分页相关辅助函数
static void update_page_info(void);
static void handle_page_up(void);
static void handle_page_down(void);
static void render_page_info(void);

// ============================================================================
// 文本处理函数实现
// ============================================================================

static uint8_t menu_get_encoded_char_len(const char* text) {
    if (text == NULL || *text == '\0') {
        return 0U;
    }

    if ((uint8_t)*text < 128U || text[1] == '\0') {
        return 1U;
    }

    return 2U;
}

static uint16_t menu_get_encoded_char_width(const char* text) {
    if (text == NULL || *text == '\0') {
        return 0U;
    }

    return ((uint8_t)*text < 128U) ? default_layout.en_char_width : default_layout.cn_char_width;
}

// 文本宽度计算辅助函数
uint16_t menu_calculate_text_width(const char* text) {
    uint16_t width = 0;
    const char* p = text;

    while (*p) {
        uint8_t char_len = menu_get_encoded_char_len(p);
        width += menu_get_encoded_char_width(p);
        p += (char_len > 0U) ? char_len : 1U;
    }
    return width;
}

// 自动换行函数：将文本分割为多行，确保每行不超过最大宽度
// 返回实际使用的行数（1或2）
uint8_t menu_wrap_text_lines(char line1[32], char line2[32], const char* text, uint16_t max_width) {
    uint16_t current_width = 0;
    uint16_t byte_count = 0;
    uint8_t line_index = 0;
    const char* src = text;
    char* current_line = line1;
    const uint16_t max_line_bytes = 31U;

    while (*src && line_index < 2) {
        uint8_t char_len = menu_get_encoded_char_len(src);
        uint16_t char_width = menu_get_encoded_char_width(src);

        // 检查添加当前字符后是否会超出宽度
        if (current_width + char_width > max_width || byte_count + char_len > max_line_bytes) {
            // 如果已经超过一行，需要换行
            if (line_index == 0) {
                current_line[byte_count] = '\0';

                // 准备第二行
                line_index = 1;
                current_line = line2;
                byte_count = 0;
                current_width = 0;

                // 重新开始处理当前字符
                continue;
            } else {
                if (byte_count + 3U <= max_line_bytes &&
                    current_width + default_layout.en_char_width * 3U <= max_width) {
                    current_line[byte_count++] = '.';
                    current_line[byte_count++] = '.';
                    current_line[byte_count++] = '.';
                }
                current_line[byte_count] = '\0';
                break;
            }
        }

        while (char_len > 0U) {
            current_line[byte_count++] = *src++;
            char_len--;
        }
        current_width += char_width;
    }

    // 设置当前行结束
    if (line_index == 0) {
        current_line[byte_count] = '\0';
        line2[0] = '\0';  // 第二行为空
        return 1;
    } else {
        current_line[byte_count] = '\0';
        return 2;
    }
}

// 计算文本布局
void menu_calculate_text_layout(TextLayout* layout, const char* text, uint16_t max_width) {
    layout->line_count = menu_wrap_text_lines(layout->line1, layout->line2, text, max_width);
    layout->total_height = layout->line_count * default_layout.line_height;
}

// 检查文本是否需要换行显示
bool menu_needs_wrapping(const char* text, uint16_t max_width) {
    return menu_calculate_text_width(text) > max_width;
}

// ============================================================================
// 菜单项查找函数
// ============================================================================

// 根据ID获取菜单项
static MenuItem* get_menu_item(uint16_t id) {
    if (menu_items == NULL) return NULL;

    for (uint16_t i = 0; i < menu_item_count; i++) {
        if (menu_items[i].id == id) {
            return (MenuItem*)&menu_items[i];
        }
    }
    return NULL;
}

// 获取指定父菜单的第一个子项
static MenuItem* get_first_child(uint16_t parent_id) {
    MenuItem* parent = get_menu_item(parent_id);
    if (parent == NULL || parent->first_child_id == 0) {
        return NULL;
    }
    return get_menu_item(parent->first_child_id);
}

// 获取当前项的下一个兄弟项
static MenuItem* get_next_sibling(uint16_t current_id) {
    MenuItem* current = get_menu_item(current_id);
    if (current == NULL || current->next_sibling_id == 0) {
        return NULL;
    }
    return get_menu_item(current->next_sibling_id);
}

// 计算指定父菜单的子项数量
static uint16_t count_children(uint16_t parent_id) {
    uint16_t count = 0;
    MenuItem* child = get_first_child(parent_id);

    while (child != NULL) {
        count++;
        child = get_next_sibling(child->id);
    }

    return count;
}

// 获取指定父菜单的第index个子项
static MenuItem* get_child_at_index(uint16_t parent_id, uint16_t index) {
    MenuItem* child = get_first_child(parent_id);

    while (child != NULL && index > 0) {
        child = get_next_sibling(child->id);
        index--;
    }

    return child;
}

// 获取指定子项在父菜单中的索引
static uint16_t get_child_index(uint16_t parent_id, uint16_t child_id) {
    uint16_t index = 0;
    MenuItem* child = get_first_child(parent_id);

    while (child != NULL) {
        if (child->id == child_id) {
            return index;
        }

        child = get_next_sibling(child->id);
        index++;
    }

    return UINT16_MAX;
}

// 计算菜单项的最大文本显示宽度
static uint16_t get_menu_item_max_text_width(const MenuItem* item, uint8_t text_x) {
    uint16_t max_text_width = JLXLCD_W - text_x - 16;

    if (item == NULL) {
        return max_text_width;
    }

    switch (item->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        max_text_width -= 20;
        break;

    case MENU_ITEM_TYPE_TOGGLE:
    case MENU_ITEM_TYPE_MOMENTARY:
        max_text_width -= 32;
        break;

    case MENU_ITEM_TYPE_VALUE:
        max_text_width -= 48;
        break;

    case MENU_ITEM_TYPE_LIST:
        max_text_width -= 64;
        break;

    default:
        break;
    }

    return max_text_width;
}

// 获取菜单项实际显示高度
static uint8_t get_menu_item_height(MenuItem* item) {
    TextLayout text_layout;
    const char* text;
    uint8_t text_x = 16;

    if (item == NULL) {
        return default_layout.item_height;
    }

    text = get_string(item->text_id);
    if (text == NULL || text[0] == '\0') {
        return default_layout.item_height;
    }

    if (item->icon_id != 0) {
        text_x = 30;
    }

    menu_calculate_text_layout(&text_layout, text, get_menu_item_max_text_width(item, text_x));
    if (text_layout.total_height == 0) {
        return default_layout.item_height;
    }

    if (text_layout.total_height > default_layout.menu_area_height) {
        return default_layout.menu_area_height;
    }

    return text_layout.total_height;
}

// 从指定索引开始构建一页，返回该页实际包含的项数
static uint8_t build_page_from_index(uint16_t parent_id, uint16_t start_index,
                                     uint16_t* page_height) {
    uint16_t total_height = 0;
    uint8_t item_count = 0;
    MenuItem* item = get_child_at_index(parent_id, start_index);

    while (item != NULL) {
        uint8_t item_height = get_menu_item_height(item);

        if (item_count > 0 && (total_height + item_height > default_layout.menu_area_height)) {
            break;
        }

        total_height += item_height;
        item_count++;
        item = get_next_sibling(item->id);
    }

    if (page_height != NULL) {
        *page_height = total_height;
    }

    return item_count;
}

// 按页码获取页面边界信息
static bool get_page_bounds_by_number(uint16_t parent_id, uint8_t page_number,
                                      uint16_t* page_start_index, uint8_t* items_in_page,
                                      uint8_t* total_pages) {
    uint16_t child_count = count_children(parent_id);
    uint16_t current_index = 0;
    uint8_t current_page = 1;

    if (child_count == 0) {
        if (page_start_index != NULL) {
            *page_start_index = 0;
        }
        if (items_in_page != NULL) {
            *items_in_page = 0;
        }
        if (total_pages != NULL) {
            *total_pages = 1;
        }
        return (page_number <= 1);
    }

    while (current_index < child_count) {
        uint8_t page_items = build_page_from_index(parent_id, current_index, NULL);

        if (page_items == 0) {
            page_items = 1;
        }

        if (current_page == page_number) {
            if (page_start_index != NULL) {
                *page_start_index = current_index;
            }
            if (items_in_page != NULL) {
                *items_in_page = page_items;
            }
        }

        current_index += page_items;
        current_page++;
    }

    if (total_pages != NULL) {
        *total_pages = current_page - 1;
    }

    return (page_number < current_page);
}

// 保存当前菜单状态到历史记录栈
static void push_history_snapshot(void) {
    uint8_t history_index = menu_ctx.history_top;

    if (history_index >= 8) {
        history_index = 7;
        menu_ctx.history_top = 7;
    }

    menu_ctx.history_stack[history_index] = menu_ctx.current_menu_id;
    menu_ctx.history_item_stack[history_index] = menu_ctx.current_item_id;

    if (menu_ctx.history_top < 8) {
        menu_ctx.history_top++;
    }
}

// 从历史记录栈恢复上一个菜单状态
static bool pop_history_snapshot(uint16_t* menu_id, uint16_t* item_id) {
    if (menu_ctx.history_top == 0) {
        return false;
    }

    menu_ctx.history_top--;

    if (menu_id != NULL) {
        *menu_id = menu_ctx.history_stack[menu_ctx.history_top];
    }
    if (item_id != NULL) {
        *item_id = menu_ctx.history_item_stack[menu_ctx.history_top];
    }

    return true;
}

// 选择指定项并刷新显示（更新分页信息）
static void select_item_and_refresh(uint16_t item_id) {
    menu_ctx.current_item_id = item_id;
    update_page_info();
    menu_ctx.need_redraw = true;
}

// 进入指定菜单，save_history参数决定是否保存当前状态到历史记录栈
static bool enter_menu_internal(uint16_t menu_id, bool save_history) {
    MenuItem* target_menu = get_menu_item(menu_id);
    MenuItem* first_child;

    if (target_menu == NULL || target_menu->type != MENU_ITEM_TYPE_SUBMENU) {
        return false;
    }

    if (save_history) {
        push_history_snapshot();
    }

    menu_ctx.current_menu_id = menu_id;
    menu_ctx.scroll_position = 0;
    menu_ctx.special_view = MENU_SPECIAL_VIEW_NONE;
    menu_ctx.io_current_page = 0;
    menu_ctx.io_total_pages = 0;
    menu_ctx.io_selected_index = 0;
    menu_ctx.io_is_editing = false;
    menu_ctx.io_edit_flash_tick = 0;
    menu_ctx.io_edit_flash_inverse = false;

    first_child = get_first_child(menu_id);
    menu_ctx.current_item_id = (first_child != NULL) ? first_child->id : 0;

    update_page_info();
    menu_register_current_page_polling(menu_id);
    menu_ctx.need_redraw = true;
    return true;
}

// 相对翻页，page_offset为正表示向后翻页，负表示向前翻页
static bool navigate_relative_page(int8_t page_offset) {
    uint16_t current_index;
    uint16_t target_page_start_index = 0;
    uint8_t target_items_in_page = 0;
    uint16_t relative_index;
    uint8_t target_page;

    if (page_offset == 0) {
        return false;
    }

    if ((page_offset < 0 && menu_ctx.current_page <= 1) ||
        (page_offset > 0 && menu_ctx.current_page >= menu_ctx.total_pages)) {
        return false;
    }

    current_index = get_child_index(menu_ctx.current_menu_id, menu_ctx.current_item_id);
    if (current_index == UINT16_MAX) {
        return false;
    }

    relative_index = current_index - menu_ctx.page_start_index;
    target_page = (uint8_t)(menu_ctx.current_page + page_offset);

    if (get_page_bounds_by_number(menu_ctx.current_menu_id, target_page, &target_page_start_index,
                                  &target_items_in_page, NULL) &&
        target_items_in_page > 0) {
        uint16_t target_index = target_page_start_index + relative_index;

        if (relative_index >= target_items_in_page) {
            target_index = target_page_start_index + target_items_in_page - 1;
        }

        MenuItem* target_item = get_child_at_index(menu_ctx.current_menu_id, target_index);
        if (target_item != NULL) {
            select_item_and_refresh(target_item->id);
            return true;
        }
    }

    return false;
}

static bool io_monitor_is_configured(void) {
    return ((io_monitor_config.x_points != NULL && io_monitor_config.x_count > 0U) ||
            (io_monitor_config.y_points != NULL && io_monitor_config.y_count > 0U));
}

static bool alarm_log_is_configured(void) {
    return alarm_log_config.rs485_addr != MENU_RS485_ADDR_NONE;
}

static StringID alarm_log_value_to_string_id(uint16_t alarm_value) {
    switch (alarm_value) {
    case 1:
        return STR_ALARM_E01;
    case 2:
        return STR_ALARM_E02;
    case 3:
        return STR_ALARM_E03;
    case 4:
        return STR_ALARM_E04;
    case 5:
        return STR_ALARM_E05;
    case 6:
        return STR_ALARM_E06;
    case 7:
        return STR_ALARM_E07;
    case 8:
        return STR_ALARM_E08;
    case 9:
        return STR_ALARM_E09;
    case 10:
        return STR_ALARM_E10;
    case 11:
        return STR_ALARM_E11;
    case 12:
        return STR_ALARM_E12;
    case 13:
        return STR_ALARM_E13;
    case 14:
        return STR_ALARM_E14;
    case 15:
        return STR_ALARM_E15;
    case 16:
        return STR_ALARM_E16;
    case 17:
        return STR_ALARM_E17;
    case 18:
        return STR_ALARM_E18;
    case 19:
        return STR_ALARM_E19;
    case 20:
        return STR_ALARM_E20;
    case 21:
        return STR_ALARM_E21;
    case 22:
        return STR_ALARM_E22;
    case 23:
        return STR_ALARM_E23;
    case 24:
        return STR_ALARM_E24;
    case 25:
        return STR_ALARM_E25;
    case 26:
        return STR_ALARM_E26;
    case 27:
        return STR_ALARM_E27;
    case 28:
        return STR_ALARM_E28;
    case 29:
        return STR_ALARM_E29;
    case 30:
        return STR_ALARM_E30;
    case 31:
        return STR_ALARM_E31;
    case 32:
        return STR_ALARM_E32;
    case 35:
        return STR_ALARM_E35;
    case 36:
        return STR_ALARM_E36;
    case 37:
        return STR_ALARM_E37;
    case 255:
        return STR_ALARM_E255;
    default:
        return STR_UNKNOWN_ERROR;
    }
}

static uint8_t alarm_log_get_entry_height(const AlarmLogEntry* entry) {
    TextLayout text_layout;
    const char* alarm_text;

    if (entry == NULL) {
        return default_layout.line_height;
    }

    alarm_text = get_string(entry->text_id);
    if (alarm_text == NULL) {
        alarm_text = "*";
    }

    menu_calculate_text_layout(&text_layout, alarm_text, ALARM_LOG_TEXT_WIDTH);
    if (text_layout.total_height == 0U) {
        return default_layout.line_height;
    }

    return text_layout.total_height;
}

static uint8_t alarm_log_build_page(uint8_t start_index, uint16_t* page_height) {
    uint16_t total_height = 0U;
    uint8_t count = 0U;
    uint16_t available_height;

    if (alarm_log_count == 0U || start_index >= alarm_log_count) {
        if (page_height != NULL) {
            *page_height = 0U;
        }
        return 0U;
    }

    available_height =
        (default_layout.menu_area_height > default_layout.line_height)
            ? (uint16_t)(default_layout.menu_area_height - default_layout.line_height)
            : default_layout.line_height;

    while ((uint16_t)(start_index + count) < alarm_log_count) {
        AlarmLogEntry* entry = alarm_log_get_entry_by_display_index((uint8_t)(start_index + count));
        uint8_t entry_height = alarm_log_get_entry_height(entry);

        if (count > 0U && (total_height + entry_height > available_height)) {
            break;
        }

        total_height += entry_height;
        count++;
    }

    if (page_height != NULL) {
        *page_height = total_height;
    }

    return count;
}

static bool alarm_log_get_page_bounds(uint8_t page_number, uint8_t* page_start_index,
                                      uint8_t* entries_in_page, uint8_t* total_pages) {
    uint8_t current_start = 0U;
    uint8_t current_page = 1U;

    if (alarm_log_count == 0U) {
        if (page_start_index != NULL) {
            *page_start_index = 0U;
        }
        if (entries_in_page != NULL) {
            *entries_in_page = 0U;
        }
        if (total_pages != NULL) {
            *total_pages = 1U;
        }
        return (page_number <= 1U);
    }

    while (current_start < alarm_log_count) {
        uint8_t page_entries = alarm_log_build_page(current_start, NULL);

        if (page_entries == 0U) {
            page_entries = 1U;
        }

        if (current_page == page_number) {
            if (page_start_index != NULL) {
                *page_start_index = current_start;
            }
            if (entries_in_page != NULL) {
                *entries_in_page = page_entries;
            }
        }

        current_start = (uint8_t)(current_start + page_entries);
        current_page++;
    }

    if (total_pages != NULL) {
        *total_pages = (uint8_t)(current_page - 1U);
    }

    return (page_number < current_page);
}

static uint8_t alarm_log_get_page_entry_count(uint8_t page_index) {
    uint8_t page_start_index = 0U;
    uint8_t entries_in_page = 0U;

    if (!alarm_log_get_page_bounds((uint8_t)(page_index + 1U), &page_start_index, &entries_in_page,
                                   NULL)) {
        return 0U;
    }

    return entries_in_page;
}

static AlarmLogEntry* alarm_log_get_entry_by_display_index(uint8_t display_index) {
    uint8_t physical_index;

    if (display_index >= alarm_log_count) {
        return NULL;
    }

    physical_index = (uint8_t)((alarm_log_head + ALARM_LOG_MAX_ENTRIES - 1U - display_index) %
                               ALARM_LOG_MAX_ENTRIES);
    if (!alarm_log_entries[physical_index].valid) {
        return NULL;
    }

    return &alarm_log_entries[physical_index];
}

static void alarm_log_register_polling(void) {
    uint16_t addr;
    MenuItemType type;

    if (!alarm_log_is_configured()) {
        return;
    }

    addr = alarm_log_config.rs485_addr;
    type = alarm_log_config.rs485_type;
    PollManager_RegisterScreenAddresses(alarm_log_config.menu_id, &addr, &type, 1U);
    PollManager_SetActiveScreen(alarm_log_config.menu_id);
}

static void alarm_log_sync_paging(void) {
    uint8_t page_entry_count;
    uint8_t page_start_index = 0U;
    uint8_t total_pages = 1U;

    (void)alarm_log_get_page_bounds(1U, NULL, NULL, &total_pages);
    alarm_log_total_pages = total_pages;

    if (alarm_log_current_page >= alarm_log_total_pages) {
        alarm_log_current_page = (uint8_t)(alarm_log_total_pages - 1U);
    }

    if (!alarm_log_get_page_bounds((uint8_t)(alarm_log_current_page + 1U), &page_start_index,
                                   &page_entry_count, &alarm_log_total_pages)) {
        alarm_log_total_pages = 1U;
        alarm_log_page_start_index = 0U;
        alarm_log_selected_row = 0U;
        return;
    }

    alarm_log_page_start_index = page_start_index;
    if (alarm_log_selected_row > page_entry_count) {
        alarm_log_selected_row = page_entry_count;
    }
}

static void alarm_log_mark_all_read(void) {
    uint8_t i;

    for (i = 0U; i < ALARM_LOG_MAX_ENTRIES; i++) {
        if (alarm_log_entries[i].valid) {
            alarm_log_entries[i].unread = false;
        }
    }
}

static void alarm_log_append(uint16_t alarm_value) {
    AlarmLogEntry* entry;
    struct realtime_t* rtc_time;

    rtc_time = bsp_RTCGetTime();
    entry = &alarm_log_entries[alarm_log_head];
    memset(entry, 0, sizeof(*entry));
    entry->valid = true;
    entry->alarm_value = alarm_value;
    entry->text_id = alarm_log_value_to_string_id(alarm_value);
    entry->unread = true;

    if (rtc_time != NULL) {
        entry->hour = rtc_time->hour;
        entry->minute = rtc_time->minute;
        entry->second = rtc_time->second;
    }

    alarm_log_head = (uint8_t)((alarm_log_head + 1U) % ALARM_LOG_MAX_ENTRIES);
    if (alarm_log_count < ALARM_LOG_MAX_ENTRIES) {
        alarm_log_count++;
    }

    alarm_log_current_page = 0U;
    alarm_log_sync_paging();
    alarm_log_selected_row = (alarm_log_get_page_entry_count(0U) > 0U) ? 1U : 0U;
}

static uint8_t io_monitor_get_total_pages_internal(void) {
    uint8_t x_pages = 0;
    uint8_t y_pages = 0;

    if (io_monitor_config.x_count > 0U) {
        x_pages = (uint8_t)((io_monitor_config.x_count + IO_MONITOR_PAGE_SIZE - 1U) /
                            IO_MONITOR_PAGE_SIZE);
    }
    if (io_monitor_config.y_count > 0U) {
        y_pages = (uint8_t)((io_monitor_config.y_count + IO_MONITOR_PAGE_SIZE - 1U) /
                            IO_MONITOR_PAGE_SIZE);
    }

    return (uint8_t)(x_pages + y_pages);
}

static bool io_monitor_page_is_output(uint8_t page_index) {
    uint8_t x_pages = 0;

    if (io_monitor_config.x_count > 0U) {
        x_pages = (uint8_t)((io_monitor_config.x_count + IO_MONITOR_PAGE_SIZE - 1U) /
                            IO_MONITOR_PAGE_SIZE);
    }

    return (page_index >= x_pages);
}

static uint8_t io_monitor_get_page_count(uint8_t page_index) {
    bool is_output;
    uint8_t local_page;
    uint8_t count;

    if (!io_monitor_is_configured()) {
        return 0;
    }

    is_output = io_monitor_page_is_output(page_index);
    if (is_output) {
        uint8_t x_pages = (io_monitor_config.x_count == 0U)
                              ? 0U
                              : (uint8_t)((io_monitor_config.x_count + IO_MONITOR_PAGE_SIZE - 1U) /
                                          IO_MONITOR_PAGE_SIZE);
        local_page = (uint8_t)(page_index - x_pages);
        count = io_monitor_config.y_count;
    } else {
        local_page = page_index;
        count = io_monitor_config.x_count;
    }

    if ((uint16_t)local_page * IO_MONITOR_PAGE_SIZE >= count) {
        return 0;
    }

    count = (uint8_t)(count - ((uint8_t)(local_page * IO_MONITOR_PAGE_SIZE)));
    if (count > IO_MONITOR_PAGE_SIZE) {
        count = IO_MONITOR_PAGE_SIZE;
    }

    return count;
}

static const IOMonitorPoint* io_monitor_get_point(uint8_t page_index, uint8_t slot,
                                                  bool* is_output) {
    bool page_is_output;
    uint8_t local_page;
    uint16_t global_index;
    uint8_t x_pages = 0;

    if (!io_monitor_is_configured() || slot >= IO_MONITOR_PAGE_SIZE) {
        return NULL;
    }

    if (io_monitor_config.x_count > 0U) {
        x_pages = (uint8_t)((io_monitor_config.x_count + IO_MONITOR_PAGE_SIZE - 1U) /
                            IO_MONITOR_PAGE_SIZE);
    }

    page_is_output = io_monitor_page_is_output(page_index);
    if (is_output != NULL) {
        *is_output = page_is_output;
    }

    if (page_is_output) {
        local_page = (uint8_t)(page_index - x_pages);
        global_index = (uint16_t)local_page * IO_MONITOR_PAGE_SIZE + slot;
        if (global_index >= io_monitor_config.y_count) {
            return NULL;
        }
        return &io_monitor_config.y_points[global_index];
    }

    local_page = page_index;
    global_index = (uint16_t)local_page * IO_MONITOR_PAGE_SIZE + slot;
    if (global_index >= io_monitor_config.x_count) {
        return NULL;
    }

    return &io_monitor_config.x_points[global_index];
}

static void io_monitor_register_all_polling(void) {
    uint16_t addrs[MAX_ADDR_PER_SCREEN];
    MenuItemType types[MAX_ADDR_PER_SCREEN];
    uint8_t count = 0;
    uint8_t i;

    if (!io_monitor_is_configured()) {
        return;
    }

    for (i = 0; i < io_monitor_config.x_count && count < MAX_ADDR_PER_SCREEN; i++) {
        if (io_monitor_config.x_points[i].rs485_addr == MENU_RS485_ADDR_NONE) {
            continue;
        }
        addrs[count] = io_monitor_config.x_points[i].rs485_addr;
        types[count] = MENU_TYPE_VALUE_BIT;
        count++;
    }

    for (i = 0; i < io_monitor_config.y_count && count < MAX_ADDR_PER_SCREEN; i++) {
        if (io_monitor_config.y_points[i].rs485_addr == MENU_RS485_ADDR_NONE) {
            continue;
        }
        addrs[count] = io_monitor_config.y_points[i].rs485_addr;
        types[count] = MENU_TYPE_VALUE_BIT;
        count++;
    }

    PollManager_RegisterScreenAddresses(menu_ctx.current_item_id, addrs, types, count);
    PollManager_SetActiveScreen(menu_ctx.current_item_id);
}

static void io_monitor_sync_paging(void) {
    uint8_t page_count;

    menu_ctx.io_total_pages = io_monitor_get_total_pages_internal();
    if (menu_ctx.io_total_pages == 0U) {
        menu_ctx.io_total_pages = 1U;
    }

    if (menu_ctx.io_current_page >= menu_ctx.io_total_pages) {
        menu_ctx.io_current_page = (uint8_t)(menu_ctx.io_total_pages - 1U);
    }

    page_count = io_monitor_get_page_count(menu_ctx.io_current_page);
    if (page_count == 0U) {
        menu_ctx.io_selected_index = 0U;
    } else if (menu_ctx.io_selected_index >= page_count) {
        menu_ctx.io_selected_index = (uint8_t)(page_count - 1U);
    }
}

static void io_monitor_close(void) {
    menu_ctx.special_view = MENU_SPECIAL_VIEW_NONE;
    menu_ctx.io_current_page = 0U;
    menu_ctx.io_total_pages = 0U;
    menu_ctx.io_selected_index = 0U;
    menu_ctx.io_is_editing = false;
    menu_ctx.io_edit_flash_tick = 0U;
    menu_ctx.io_edit_flash_inverse = false;
    menu_ctx.current_state = MENU_STATE_BROWSING;
    menu_register_current_page_polling(menu_ctx.current_menu_id);
    menu_ctx.need_redraw = true;
}

static void io_monitor_move_up(void) {
    uint8_t page_count;

    if (menu_ctx.io_is_editing) {
        return;
    }

    page_count = io_monitor_get_page_count(menu_ctx.io_current_page);
    if (page_count == 0U) {
        return;
    }

    if (menu_ctx.io_selected_index > 0U) {
        menu_ctx.io_selected_index--;
        menu_ctx.need_redraw = true;
        return;
    }

    if (menu_ctx.io_current_page > 0U) {
        menu_ctx.io_current_page--;
        io_monitor_sync_paging();

        page_count = io_monitor_get_page_count(menu_ctx.io_current_page);
        menu_ctx.io_selected_index = (page_count > 0U) ? (uint8_t)(page_count - 1U) : 0U;
        menu_ctx.need_redraw = true;
    }
}

static void io_monitor_move_down(void) {
    uint8_t page_count = io_monitor_get_page_count(menu_ctx.io_current_page);

    if (menu_ctx.io_is_editing || page_count == 0U) {
        return;
    }

    if (menu_ctx.io_selected_index + 1U < page_count) {
        menu_ctx.io_selected_index++;
        menu_ctx.need_redraw = true;
        return;
    }

    if (menu_ctx.io_current_page + 1U < menu_ctx.io_total_pages) {
        menu_ctx.io_current_page++;
        io_monitor_sync_paging();
        menu_ctx.io_selected_index = 0U;
        menu_ctx.need_redraw = true;
    }
}

static void io_monitor_page_up(void) {
    if (menu_ctx.io_is_editing) {
        return;
    }

    if (menu_ctx.io_current_page > 0U) {
        menu_ctx.io_current_page--;
        io_monitor_sync_paging();
        menu_ctx.need_redraw = true;
    }
}

static void io_monitor_page_down(void) {
    if (menu_ctx.io_is_editing) {
        return;
    }

    if (menu_ctx.io_current_page + 1U < menu_ctx.io_total_pages) {
        menu_ctx.io_current_page++;
        io_monitor_sync_paging();
        menu_ctx.need_redraw = true;
    }
}

static void io_monitor_select(void) {
    const IOMonitorPoint* point;
    bool is_output = false;
    bool current_value = false;

    point = io_monitor_get_point(menu_ctx.io_current_page, menu_ctx.io_selected_index, &is_output);
    if (point == NULL || !is_output || point->rs485_addr == MENU_RS485_ADDR_NONE) {
        return;
    }

    if (!menu_ctx.io_is_editing) {
        menu_ctx.io_is_editing = true;
        menu_ctx.io_edit_flash_tick = bsp_get_systick();
        menu_ctx.io_edit_flash_inverse = true;
        menu_ctx.need_redraw = true;
        return;
    }

    if (!HashCache_GetBit(point->rs485_addr, &current_value)) {
        current_value = false;
    }

    if (WriteValue(point->rs485_addr, 1U, current_value ? 0 : 1)) {
        menu_ctx.io_is_editing = false;
        menu_ctx.io_edit_flash_tick = 0U;
        menu_ctx.io_edit_flash_inverse = false;
        menu_ctx.need_redraw = true;
    }
}

static void io_monitor_back(void) {
    if (menu_ctx.io_is_editing) {
        menu_ctx.io_is_editing = false;
        menu_ctx.io_edit_flash_tick = 0U;
        menu_ctx.io_edit_flash_inverse = false;
        menu_ctx.need_redraw = true;
        return;
    }

    io_monitor_close();
}

static bool alarm_log_open(bool save_history) {
    if (!alarm_log_is_configured()) {
        return false;
    }

    if (save_history && menu_ctx.special_view != MENU_SPECIAL_VIEW_ALARM_LOG) {
        push_history_snapshot();
    }

    menu_ctx.special_view = MENU_SPECIAL_VIEW_ALARM_LOG;
    menu_ctx.current_menu_id = alarm_log_config.menu_id;
    menu_ctx.current_item_id = alarm_log_config.menu_id;
    menu_ctx.is_editing = false;
    menu_ctx.edit_item_id = 0U;
    menu_ctx.edit_original_value = 0;
    menu_ctx.edit_pending_value = 0;
    menu_ctx.edit_value_step_index = 0U;
    menu_ctx.edit_flash_tick = 0U;
    menu_ctx.edit_flash_inverse = false;
    menu_ctx.current_state = MENU_STATE_BROWSING;

    alarm_log_sync_paging();
    if (alarm_log_get_page_entry_count(alarm_log_current_page) > 0U &&
        alarm_log_selected_row == 0U) {
        alarm_log_selected_row = 1U;
    }

    alarm_log_register_polling();
    menu_ctx.need_redraw = true;
    return true;
}

static void alarm_log_close(void) {
    uint16_t previous_menu_id;
    uint16_t previous_item_id;

    menu_ctx.special_view = MENU_SPECIAL_VIEW_NONE;
    menu_ctx.current_state = MENU_STATE_BROWSING;

    if (pop_history_snapshot(&previous_menu_id, &previous_item_id)) {
        menu_ctx.current_menu_id = previous_menu_id;
        select_item_and_refresh(previous_item_id);
        menu_register_current_page_polling(previous_menu_id);
    } else {
        menu_register_current_page_polling(menu_ctx.current_menu_id);
        menu_ctx.need_redraw = true;
    }
}

static void alarm_log_move_up(void) {
    if (alarm_log_selected_row > 0U) {
        alarm_log_selected_row--;
        menu_ctx.need_redraw = true;
        return;
    }

    if (alarm_log_current_page > 0U) {
        alarm_log_current_page--;
        alarm_log_sync_paging();
        alarm_log_selected_row = alarm_log_get_page_entry_count(alarm_log_current_page);
        menu_ctx.need_redraw = true;
    }
}

static void alarm_log_move_down(void) {
    uint8_t page_entry_count = alarm_log_get_page_entry_count(alarm_log_current_page);

    if (alarm_log_selected_row < page_entry_count) {
        alarm_log_selected_row++;
        menu_ctx.need_redraw = true;
        return;
    }

    if (alarm_log_current_page + 1U < alarm_log_total_pages) {
        alarm_log_current_page++;
        alarm_log_sync_paging();
        page_entry_count = alarm_log_get_page_entry_count(alarm_log_current_page);
        alarm_log_selected_row = (page_entry_count > 0U) ? 1U : 0U;
        menu_ctx.need_redraw = true;
    }
}

static void alarm_log_page_up(void) {
    uint8_t page_entry_count;

    if (alarm_log_current_page == 0U) {
        return;
    }

    alarm_log_current_page--;
    alarm_log_sync_paging();
    page_entry_count = alarm_log_get_page_entry_count(alarm_log_current_page);
    if (alarm_log_selected_row > page_entry_count) {
        alarm_log_selected_row = page_entry_count;
    }
    menu_ctx.need_redraw = true;
}

static void alarm_log_page_down(void) {
    uint8_t page_entry_count;

    if (alarm_log_current_page + 1U >= alarm_log_total_pages) {
        return;
    }

    alarm_log_current_page++;
    alarm_log_sync_paging();
    page_entry_count = alarm_log_get_page_entry_count(alarm_log_current_page);
    if (alarm_log_selected_row > page_entry_count) {
        alarm_log_selected_row = page_entry_count;
    }
    if (alarm_log_selected_row == 0U && page_entry_count > 0U) {
        alarm_log_selected_row = 1U;
    }
    menu_ctx.need_redraw = true;
}

static void alarm_log_select(void) {
    uint8_t display_index;
    AlarmLogEntry* entry;

    if (alarm_log_selected_row == 0U) {
        return;
    }

    display_index = (uint8_t)(alarm_log_page_start_index + alarm_log_selected_row - 1U);
    entry = alarm_log_get_entry_by_display_index(display_index);
    if (entry == NULL || !entry->unread) {
        return;
    }

    entry->unread = false;
    menu_ctx.need_redraw = true;
}

static void alarm_log_back(void) {
    alarm_log_close();
}

// ============================================================================
// 渲染函数实现
// ============================================================================

static bool menu_item_has_rs485_binding(const MenuItem* item) {
    return item != NULL && item->rs485_addr != MENU_RS485_ADDR_NONE &&
           (item->rs485_width == 1U || item->rs485_width == 2U || item->rs485_width == 4U);
}

static bool menu_item_read_rs485_value(const MenuItem* item, int32_t* value) {
    bool bit_value;
    uint16_t word_value;
    uint32_t dword_value;

    if (item == NULL || value == NULL || !menu_item_has_rs485_binding(item)) {
        return false;
    }

    if (item->rs485_width == 1U || item->rs485_type == MENU_TYPE_VALUE_BIT) {
        if (!HashCache_GetBit(item->rs485_addr, &bit_value)) {
            return false;
        }
        *value = bit_value ? 1 : 0;
        return true;
    }

    if (item->rs485_width == 4U || item->rs485_type == MENU_TYPE_VALUE_DINT ||
        item->rs485_type == MENU_TYPE_VALUE_UINT32) {
        if (!HashCache_GetDWord(item->rs485_addr, &dword_value)) {
            return false;
        }
        *value = (int32_t)dword_value;
        return true;
    }

    if (!HashCache_GetWord(item->rs485_addr, &word_value)) {
        return false;
    }

    *value = word_value;
    return true;
}

static void menu_item_update_shadow_value(const MenuItem* item, int32_t value) {
    if (item == NULL) {
        return;
    }

    if (menu_item_has_rs485_binding(item)) {
        if (item->rs485_width == 1U || item->rs485_type == MENU_TYPE_VALUE_BIT) {
            HashCache_SetBit(item->rs485_addr, value != 0);
        } else if (item->rs485_width == 4U || item->rs485_type == MENU_TYPE_VALUE_DINT ||
                   item->rs485_type == MENU_TYPE_VALUE_UINT32) {
            HashCache_SetDWord(item->rs485_addr, (uint32_t)value);
        } else {
            HashCache_SetWord(item->rs485_addr, (uint16_t)value);
        }
    }

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        if (item->data.toggle_value != NULL) {
            *(item->data.toggle_value) = (value != 0);
        }
        break;

    case MENU_ITEM_TYPE_MOMENTARY:
        if (item->data.momentary_value != NULL) {
            *(item->data.momentary_value) = (value != 0);
        }
        break;

    case MENU_ITEM_TYPE_VALUE:
        if (item->data.value_ptr != NULL) {
            *(item->data.value_ptr) = value;
        }
        break;

    case MENU_ITEM_TYPE_LIST:
        if (item->data.selection_ptr != NULL) {
            *(item->data.selection_ptr) = (uint8_t)value;
        }
        break;

    default:
        break;
    }
}

static void menu_register_cache_for_item(const MenuItem* item) {
    if (!menu_item_has_rs485_binding(item)) {
        return;
    }

    if (item->rs485_width == 1U || item->rs485_type == MENU_TYPE_VALUE_BIT) {
        HashCache_RegisterBit(item->rs485_addr);
        return;
    }

    HashCache_RegisterWord(item->rs485_addr);
    if (item->rs485_width == 4U || item->rs485_type == MENU_TYPE_VALUE_DINT ||
        item->rs485_type == MENU_TYPE_VALUE_UINT32) {
        HashCache_RegisterWord((uint16_t)(item->rs485_addr + 1U));
    }
}

static void menu_register_all_cache_items(void) {
    uint16_t i;

    for (i = 0; i < menu_item_count; i++) {
        menu_register_cache_for_item(&menu_items[i]);
    }
}

static void menu_register_current_page_polling(uint16_t menu_id) {
    uint16_t addrs[MAX_ADDR_PER_SCREEN];
    MenuItemType types[MAX_ADDR_PER_SCREEN];
    uint8_t count = 0;
    MenuItem* child = get_first_child(menu_id);

    while (child != NULL && count < MAX_ADDR_PER_SCREEN) {
        if (menu_item_has_rs485_binding(child)) {
            addrs[count] = child->rs485_addr;
            types[count] = child->rs485_type;
            count++;
        }
        child = get_next_sibling(child->id);
    }

    PollManager_RegisterScreenAddresses(menu_id, addrs, types, count);
    PollManager_SetActiveScreen(menu_id);
}

static int32_t get_editable_item_value(const MenuItem* item) {
    int32_t rs485_value;

    if (item == NULL) {
        return 0;
    }

    if (menu_item_read_rs485_value(item, &rs485_value)) {
        return rs485_value;
    }

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        return (item->data.toggle_value != NULL && *(item->data.toggle_value)) ? 1 : 0;

    case MENU_ITEM_TYPE_MOMENTARY:
        return (item->data.momentary_value != NULL && *(item->data.momentary_value)) ? 1 : 0;

    case MENU_ITEM_TYPE_VALUE:
        return (item->data.value_ptr != NULL) ? *(item->data.value_ptr) : 0;

    case MENU_ITEM_TYPE_LIST:
        return (item->data.selection_ptr != NULL) ? *(item->data.selection_ptr) : 0;

    default:
        return 0;
    }
}

static int32_t get_render_value(const MenuItem* item, bool is_selected, bool is_editing) {
    if (is_selected && is_editing && item != NULL && item->id == menu_ctx.edit_item_id) {
        return menu_ctx.edit_pending_value;
    }

    return get_editable_item_value(item);
}

static bool menu_item_set_momentary_state(MenuItem* item, bool on) {
    int32_t value;

    if (item == NULL || item->type != MENU_ITEM_TYPE_MOMENTARY) {
        return false;
    }

    value = on ? 1 : 0;

    if (menu_item_has_rs485_binding(item) &&
        !WriteValue(item->rs485_addr, item->rs485_width, value)) {
        return false;
    }

    menu_item_update_shadow_value(item, value);
    if (item->data.momentary_changed != NULL) {
        item->data.momentary_changed(on);
    }

    return true;
}

static void menu_update_momentary_enter_state(void) {
    MenuItem* current_item = get_menu_item(menu_ctx.current_item_id);
    bool is_enter_pressed = key_control_is_key_pressed(FUNKEY_ENTER);
    bool can_hold = (menu_ctx.special_view == MENU_SPECIAL_VIEW_NONE && !menu_ctx.is_editing &&
                     current_item != NULL && current_item->type == MENU_ITEM_TYPE_MOMENTARY);

    if (can_hold && is_enter_pressed) {
        if (!momentary_enter_active || momentary_active_item_id != current_item->id) {
            if (momentary_enter_active) {
                MenuItem* old_item = get_menu_item(momentary_active_item_id);
                (void)menu_item_set_momentary_state(old_item, false);
            }

            if (menu_item_set_momentary_state(current_item, true)) {
                momentary_enter_active = true;
                momentary_active_item_id = current_item->id;
                menu_ctx.need_redraw = true;
            }
        }
        return;
    }

    if (momentary_enter_active) {
        MenuItem* active_item = get_menu_item(momentary_active_item_id);
        (void)menu_item_set_momentary_state(active_item, false);
        momentary_enter_active = false;
        momentary_active_item_id = 0U;
        menu_ctx.need_redraw = true;
    }
}

static bool is_value_editing_item(const MenuItem* item) {
    return (item != NULL && item->type == MENU_ITEM_TYPE_VALUE);
}

static uint8_t get_value_step_index_by_step(int32_t step_value) {
    uint8_t i;

    for (i = 0; i < VALUE_EDIT_STEP_COUNT; i++) {
        if (value_edit_steps[i] == step_value) {
            return i;
        }
    }

    return 0;
}

static int32_t get_current_value_edit_step(void) {
    if (menu_ctx.edit_value_step_index >= VALUE_EDIT_STEP_COUNT) {
        return value_edit_steps[0];
    }

    return value_edit_steps[menu_ctx.edit_value_step_index];
}

static void switch_value_edit_step(int8_t direction) {
    MenuItem* item = get_menu_item(menu_ctx.edit_item_id);

    if (!is_value_editing_item(item) || direction == 0) {
        return;
    }

    if (direction < 0) {
        if (menu_ctx.edit_value_step_index > 0) {
            menu_ctx.edit_value_step_index--;
            menu_ctx.need_redraw = true;
        }
    } else {
        if (menu_ctx.edit_value_step_index + 1 < VALUE_EDIT_STEP_COUNT) {
            menu_ctx.edit_value_step_index++;
            menu_ctx.need_redraw = true;
        }
    }
}

static void begin_edit_session(MenuItem* item) {
    if (item == NULL) {
        return;
    }

    menu_ctx.is_editing = true;
    menu_ctx.edit_item_id = item->id;
    menu_ctx.edit_original_value = get_editable_item_value(item);
    menu_ctx.edit_pending_value = menu_ctx.edit_original_value;
    menu_ctx.edit_value_step_index =
        is_value_editing_item(item) ? get_value_step_index_by_step(item->data.step_value) : 0;
    menu_ctx.edit_flash_tick = bsp_get_systick();
    menu_ctx.edit_flash_inverse = true;
    menu_ctx.current_state = MENU_STATE_EDITING;
    menu_ctx.need_redraw = true;
}

static bool apply_pending_edit_value(MenuItem* item) {
    bool changed = false;

    if (item == NULL) {
        return false;
    }

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        if (menu_ctx.edit_original_value == menu_ctx.edit_pending_value) {
            return true;
        }

        if (menu_item_has_rs485_binding(item) &&
            !WriteValue(item->rs485_addr, item->rs485_width, menu_ctx.edit_pending_value)) {
            return false;
        }

        menu_item_update_shadow_value(item, menu_ctx.edit_pending_value);
        if (item->data.toggle_changed != NULL) {
            item->data.toggle_changed(menu_ctx.edit_pending_value != 0);
        }
        changed = true;
        break;

    case MENU_ITEM_TYPE_VALUE:
        if (menu_ctx.edit_original_value == menu_ctx.edit_pending_value) {
            return true;
        }

        if (menu_item_has_rs485_binding(item) &&
            !WriteValue(item->rs485_addr, item->rs485_width, menu_ctx.edit_pending_value)) {
            return false;
        }

        menu_item_update_shadow_value(item, menu_ctx.edit_pending_value);
        if (item->data.value_changed != NULL) {
            item->data.value_changed(menu_ctx.edit_pending_value);
        }
        changed = true;
        break;

    case MENU_ITEM_TYPE_LIST:
        if (menu_ctx.edit_original_value == menu_ctx.edit_pending_value) {
            return true;
        }

        if (menu_item_has_rs485_binding(item) &&
            !WriteValue(item->rs485_addr, item->rs485_width, menu_ctx.edit_pending_value)) {
            return false;
        }

        menu_item_update_shadow_value(item, menu_ctx.edit_pending_value);
        if (item->data.selection_changed != NULL) {
            item->data.selection_changed((uint8_t)menu_ctx.edit_pending_value);
        }
        changed = true;
        break;

    default:
        break;
    }

    return changed || (menu_ctx.edit_original_value == menu_ctx.edit_pending_value);
}

static void update_edit_flash_state(uint32_t current_tick) {
    bool inverse;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        if (!menu_ctx.io_is_editing) {
            return;
        }

        inverse = ((current_tick - menu_ctx.io_edit_flash_tick) % 1200U) < 600U;
        if (inverse != menu_ctx.io_edit_flash_inverse) {
            menu_ctx.io_edit_flash_inverse = inverse;
            menu_ctx.need_redraw = true;
        }
        return;
    }

    if (!menu_ctx.is_editing) {
        return;
    }

    inverse = ((current_tick - menu_ctx.edit_flash_tick) % 1200U) < 600U;
    if (inverse != menu_ctx.edit_flash_inverse) {
        menu_ctx.edit_flash_inverse = inverse;
        menu_ctx.need_redraw = true;
    }
}

// 渲染菜单项（支持多行文本）
static void render_menu_item_internal(uint8_t y_pos, MenuItem* item, bool is_selected,
                                      bool is_editing) {
    if (item == NULL) return;

    // 绘制选择指示器
    if (is_selected) {
        if (is_editing) {
            JLX_ShowStringAnyRow(2, y_pos, ">", default_layout.font_size, 1);
        } else {
            JLX_ShowStringAnyRow(2, y_pos, ">", default_layout.font_size, 1);
        }
    }

    // 获取文本
    const char* text = get_string(item->text_id);
    if (text == NULL) return;

    // 计算文本区域布局
    TextLayout text_layout;
    uint8_t text_x = 16;

    // 如果有图标，调整文本位置
    if (item->icon_id != 0) {
        // 注意：这里需要根据实际情况获取图标数据
        // bsp_JLXLcdShow(16, y_pos, default_layout.icon_size, default_layout.icon_size,
        //               get_icon_data(item->icon_id), 1);
        text_x = 30;  // 图标右侧开始
    }

    // 计算文本的最大可用宽度
    // 需要根据菜单项类型为附加信息留出空间
    uint16_t max_text_width = get_menu_item_max_text_width(item, text_x);

    // 计算文本布局（自动换行）
    menu_calculate_text_layout(&text_layout, text, max_text_width);

    // 绘制第一行文本
    if (text_layout.line_count > 0) {
        JLX_ShowStringAnyRow(text_x, y_pos, text_layout.line1, default_layout.font_size, 0);
    }

    // 绘制第二行文本（如果有）
    if (text_layout.line_count > 1) {
        JLX_ShowStringAnyRow(text_x, y_pos + default_layout.line_height, text_layout.line2,
                             default_layout.font_size, 0);
    }

    // 根据类型绘制附加信息
    switch (item->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        // 子菜单箭头，根据行数调整位置
        if (text_layout.line_count > 1) {
            JLX_ShowStringAnyRow(JLXLCD_W - 16, y_pos + default_layout.line_height, "->",
                                 default_layout.font_size, 0);
        } else {
            JLX_ShowStringAnyRow(JLXLCD_W - 16, y_pos, "->", default_layout.font_size, 0);
        }
        break;

    case MENU_ITEM_TYPE_TOGGLE: {
        const char* toggle_text = NULL;
        StringID* option_ids = item->data.toggle_option_ids;
        uint8_t option_count = item->data.toggle_option_count;
        uint8_t value_mode = (is_selected && is_editing && menu_ctx.edit_flash_inverse) ? 1 : 0;
        int32_t display_value = get_render_value(item, is_selected, is_editing);
        uint8_t selection = (display_value != 0) ? 1U : 0U;

        if (option_ids != NULL && selection < option_count) {
            const char* selected_text = get_string(option_ids[selection]);
            if (selected_text != NULL && selected_text[0] != '\0' && selected_text[0] != '*') {
                toggle_text = selected_text;
            }
        }
        if (toggle_text == NULL) {
            toggle_text = (selection != 0U) ? get_string(STR_YES) : get_string(STR_NO);
        }
        if (toggle_text != NULL) {
            // 根据行数调整位置
            if (text_layout.line_count > 1) {
                JLX_ShowStringAnyRow(JLXLCD_W - 32, y_pos + default_layout.line_height, toggle_text,
                                     default_layout.font_size, value_mode);
            } else {
                JLX_ShowStringAnyRow(JLXLCD_W - 32, y_pos, toggle_text, default_layout.font_size,
                                     value_mode);
            }
        }
    } break;

    case MENU_ITEM_TYPE_MOMENTARY: {
        const char* state_text = NULL;
        StringID* option_ids = item->data.momentary_option_ids;
        uint8_t option_count = item->data.momentary_option_count;
        int32_t display_value = get_render_value(item, is_selected, false);
        uint8_t selection = (display_value != 0) ? 1U : 0U;

        if (option_ids != NULL && selection < option_count) {
            const char* selected_text = get_string(option_ids[selection]);
            if (selected_text != NULL && selected_text[0] != '\0' && selected_text[0] != '*') {
                state_text = selected_text;
            }
        }
        if (state_text == NULL) {
            state_text = (selection != 0U) ? get_string(STR_YES) : get_string(STR_NO);
        }
        if (state_text != NULL) {
            if (text_layout.line_count > 1) {
                JLX_ShowStringAnyRow(JLXLCD_W - 32, y_pos + default_layout.line_height, state_text,
                                     default_layout.font_size, 0);
            } else {
                JLX_ShowStringAnyRow(JLXLCD_W - 32, y_pos, state_text, default_layout.font_size, 0);
            }
        }
    } break;

    case MENU_ITEM_TYPE_VALUE: {
        char value_str[16];
        uint8_t value_mode = (is_selected && is_editing && menu_ctx.edit_flash_inverse) ? 1 : 0;
        int32_t display_value = get_render_value(item, is_selected, is_editing);

        snprintf(value_str, sizeof(value_str), "%d", display_value);
        // 根据行数调整位置
        if (text_layout.line_count > 1) {
            JLX_ShowStringAnyRow(JLXLCD_W - 48, y_pos + default_layout.line_height, value_str,
                                 default_layout.font_size, value_mode);

            if (is_selected && is_editing) {
                // 编辑模式下高亮显示
                bsp_JLXLcdShowUnderline(JLXLCD_W - 48, y_pos + default_layout.line_height + 14, 32);
            }
        } else {
            JLX_ShowStringAnyRow(JLXLCD_W - 48, y_pos, value_str, default_layout.font_size,
                                 value_mode);

            if (is_selected && is_editing) {
                // 编辑模式下高亮显示
                bsp_JLXLcdShowUnderline(JLXLCD_W - 48, y_pos + 14, 32);
            }
        }
    } break;

    case MENU_ITEM_TYPE_LIST: {
        uint8_t value_mode = (is_selected && is_editing && menu_ctx.edit_flash_inverse) ? 1 : 0;
        int32_t display_value = get_render_value(item, is_selected, is_editing);

        if (item->data.option_ids != NULL) {
            uint8_t selection = (uint8_t)display_value;
            if (selection < item->data.option_count) {
                const char* selected_text = get_string(item->data.option_ids[selection]);
                if (selected_text != NULL) {
                    // 根据行数调整位置
                    if (text_layout.line_count > 1) {
                        JLX_ShowStringAnyRow(JLXLCD_W - 64, y_pos + default_layout.line_height,
                                             selected_text, default_layout.font_size, value_mode);

                        if (is_selected && is_editing) {
                            JLX_ShowStringAnyRow(JLXLCD_W - 16, y_pos + default_layout.line_height,
                                                 "v", default_layout.font_size, value_mode);
                        }
                    } else {
                        JLX_ShowStringAnyRow(JLXLCD_W - 64, y_pos, selected_text,
                                             default_layout.font_size, value_mode);

                        if (is_selected && is_editing) {
                            JLX_ShowStringAnyRow(JLXLCD_W - 16, y_pos, "v",
                                                 default_layout.font_size, value_mode);
                        }
                    }
                }
            }
        }
    } break;

    case MENU_ITEM_TYPE_INFO: {
        char value_str[16];
        int32_t display_value = get_render_value(item, is_selected, is_editing);
        snprintf(value_str, sizeof(value_str), "%d", display_value);
        if (text_layout.line_count > 1) {
            JLX_ShowStringAnyRow(JLXLCD_W - 48, y_pos + default_layout.line_height, value_str,
                                 default_layout.font_size, 0);
        } else {
            JLX_ShowStringAnyRow(JLXLCD_W - 48, y_pos, value_str, default_layout.font_size, 0);
        }
    } break;

    default:
        break;
    }
}

static void render_io_monitor_view(void) {
    uint8_t slot;
    uint8_t page_count = io_monitor_get_page_count(menu_ctx.io_current_page);

    JLX_ClearRectPixel(0, default_layout.menu_area_top, JLXLCD_W, default_layout.menu_area_height,
                       0);

    for (slot = 0; slot < IO_MONITOR_PAGE_SIZE; slot++) {
        const IOMonitorPoint* point;
        bool is_output = false;
        bool value = false;
        char display_text_state[12];
        char display_text_id[12];
        const char* io_name;
        uint8_t row;
        uint8_t col;
        uint16_t x;
        uint16_t y;
        uint8_t mode = 0;

        row = (uint8_t)(slot / IO_MONITOR_COLUMNS);
        col = (uint8_t)(slot % IO_MONITOR_COLUMNS);
        x = (uint16_t)(13U + col * 43U);
        y = (uint16_t)(default_layout.menu_area_top + row * 22U);

        point = io_monitor_get_point(menu_ctx.io_current_page, slot, &is_output);
        if (point == NULL) {
            continue;
        }

        if (point->rs485_addr == MENU_RS485_ADDR_NONE) {
            value = false;
        } else if (!HashCache_GetBit(point->rs485_addr, &value)) {
            value = false;
        }

        io_name = get_string(point->text_id);
        if (io_name == NULL) {
            io_name = "*";
        }

        snprintf(display_text_state, sizeof(display_text_state), "%s",
                 (point->rs485_addr == MENU_RS485_ADDR_NONE) ? "--" : (value ? "开" : "关"));
        snprintf(display_text_id, sizeof(display_text_id), "%s", io_name);

        if (slot == menu_ctx.io_selected_index && slot < page_count) {
            if (menu_ctx.io_is_editing && is_output) {
                mode = menu_ctx.io_edit_flash_inverse ? 1U : 0U;
            } else {
                mode = 1U;
            }
        }

        JLX_ShowStringAnyRow(x, y, display_text_state, default_layout.font_size, 0);
        JLX_ShowStringAnyRow(x + 15, y, display_text_id, default_layout.font_size, mode);
    }
}

static void render_alarm_log_view(void) {
    uint8_t row;
    uint8_t page_entry_count = alarm_log_get_page_entry_count(alarm_log_current_page);
    uint8_t y = (uint8_t)(default_layout.menu_area_top + default_layout.line_height);

    JLX_ClearRectPixel(0, default_layout.menu_area_top, JLXLCD_W, default_layout.menu_area_height,
                       0);

    if (alarm_log_selected_row == 0U) {
        JLX_ShowStringAnyRow(2, default_layout.menu_area_top, ">", default_layout.font_size, 0);
    }
    JLX_ShowStringAnyRow(ALARM_LOG_TIME_X, default_layout.menu_area_top, "时间",
                         default_layout.font_size, 0);
    JLX_ShowStringAnyRow(ALARM_LOG_TEXT_X, default_layout.menu_area_top, "报警内容",
                         default_layout.font_size, 0);

    if (page_entry_count == 0U) {
        JLX_ShowStringAnyRow(ALARM_LOG_TEXT_X,
                             default_layout.menu_area_top + default_layout.line_height, "--",
                             default_layout.font_size, 0);
        return;
    }

    for (row = 0U; row < page_entry_count; row++) {
        AlarmLogEntry* entry =
            alarm_log_get_entry_by_display_index((uint8_t)(alarm_log_page_start_index + row));
        const char* alarm_text;
        char time_text[16];
        TextLayout text_layout;
        uint8_t mode;

        if (entry == NULL) {
            continue;
        }

        alarm_text = get_string(entry->text_id);
        if (alarm_text == NULL) {
            alarm_text = "*";
        }

        snprintf(time_text, sizeof(time_text), "%02u:%02u:%02u", entry->hour, entry->minute,
                 entry->second);
        menu_calculate_text_layout(&text_layout, alarm_text, ALARM_LOG_TEXT_WIDTH);
        mode = entry->unread ? 1U : 0U;

        if (alarm_log_selected_row == (uint8_t)(row + 1U)) {
            JLX_ShowStringAnyRow(2, y, ">", default_layout.font_size, 0);
        }

        JLX_ShowStringAnyRow(ALARM_LOG_TIME_X, y, time_text, default_layout.font_size, 0);

        if (text_layout.line_count > 0U) {
            JLX_ShowStringAnyRow(ALARM_LOG_TEXT_X, y, text_layout.line1, default_layout.font_size,
                                 mode);
        }
        if (text_layout.line_count > 1U) {
            JLX_ShowStringAnyRow(ALARM_LOG_TEXT_X, y + default_layout.line_height,
                                 text_layout.line2, default_layout.font_size, mode);
        }

        y = (uint8_t)(y + alarm_log_get_entry_height(entry));
    }
}

// 渲染标题栏
static void render_header_internal(void) {
    // 清除标题栏区域
    JLX_ClearRectPixel(0, 0, JLXLCD_W, default_layout.header_height, 0);

    // 获取当前菜单标题
    MenuItem* current_menu = get_menu_item(menu_ctx.current_menu_id);
    const char* title = NULL;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        MenuItem* current_item = get_menu_item(menu_ctx.current_item_id);
        if (current_item != NULL) {
            title = get_string(current_item->text_id);
        }
    } else if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        MenuItem* current_item = get_menu_item(menu_ctx.current_menu_id);
        if (current_item != NULL) {
            title = get_string(current_item->text_id);
        }
    } else if (current_menu != NULL) {
        title = get_string(current_menu->text_id);
    }

    if (title != NULL) {
        JLX_ShowStringAnyRow(2, 2, title, default_layout.font_size, 0);
    }

    // 注意：电池状态和时间显示需要根据实际情况实现
    // render_battery_status();
    // render_current_time();
}

// 渲染底部状态栏
static void render_footer_internal(void) {
    uint8_t y_pos = JLXLCD_H - default_layout.footer_height;
    bool is_value_editing = false;

    // 清除底部状态栏区域
    JLX_ClearRectPixel(0, y_pos, JLXLCD_W, default_layout.footer_height, 0);

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        bool output_page = io_monitor_page_is_output(menu_ctx.io_current_page);

        if (menu_ctx.io_is_editing) {
            JLX_ShowStringAnyRow(2, y_pos + 4, "确认:切换   返回:取消", default_layout.font_size,
                                 0);
        } else if (output_page) {
            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:选择 确认:编辑", default_layout.font_size, 0);
        } else {
            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:选择 左/右:翻页", default_layout.font_size,
                                 0);
        }

        if (menu_ctx.io_total_pages > 1U) {
            render_page_info();
        }
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        if (alarm_log_count == 0U) {
            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:选择 左/右:翻页", default_layout.font_size,
                                 0);
        } else if (alarm_log_selected_row == 0U) {
            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:选择 左/右:翻页", default_layout.font_size,
                                 0);
        } else {
            JLX_ShowStringAnyRow(2, y_pos + 4, "确认:已读 左/右:翻页", default_layout.font_size, 0);
        }

        if (alarm_log_total_pages > 1U) {
            render_page_info();
        }
        return;
    }

    // 绘制帮助文本
    MenuItem* current_item = get_menu_item(menu_ctx.current_item_id);
    if (current_item && current_item->help_id != STR_NONE) {
        const char* help_text = get_string(current_item->help_id);
        if (help_text != NULL) {
            JLX_ShowStringAnyRow(2, y_pos + 4, help_text, default_layout.font_size, 0);
        }
    }

    // 绘制操作提示
    if (menu_ctx.is_editing) {
        MenuItem* edit_item = get_menu_item(menu_ctx.edit_item_id);
        is_value_editing = is_value_editing_item(edit_item);

        if (is_value_editing) {
            char step_str[20];
            uint16_t step_width;
            uint16_t step_x;

            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:调整   左/右:", default_layout.font_size, 0);

            snprintf(step_str, sizeof(step_str), "步长:%d", (int)get_current_value_edit_step());
            // step_width = menu_calculate_text_width(step_str);
            // step_x = (step_width + 2U < JLXLCD_W) ? (JLXLCD_W - step_width - 2U) : 0;
            JLX_ShowStringAnyRow(120, y_pos + 4, step_str, default_layout.font_size, 0);
        } else {
            JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:调整", default_layout.font_size, 0);
        }
        // JLX_ShowStringAnyRow(JLXLCD_W - 64, y_pos + 4, "确认:√ 取消:X", default_layout.font_size,
        //                      0);
    } else {
        JLX_ShowStringAnyRow(2, y_pos + 4, "上/下:选择   左/右:翻页", default_layout.font_size, 0);
        // JLX_ShowStringAnyRow(JLXLCD_W - 48, y_pos + 4, "确认:→ 返回:←", default_layout.font_size,
        //                      0);
    }

    // 显示页码信息（仅在多页时显示）
    if (menu_ctx.total_pages > 1 && !is_value_editing) {
        render_page_info();
    }
}

// 完整渲染函数
static void render_full(void) {
    uint16_t rendered_height = 0;
    uint8_t rendered_items = 0;

    // 渲染标题栏
    render_header_internal();

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        render_io_monitor_view();
    } else if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        render_alarm_log_view();
    } else {
        // 清除菜单区域，按动态高度重新布局整页内容
        JLX_ClearRectPixel(0, default_layout.menu_area_top, JLXLCD_W,
                           default_layout.menu_area_height, 0);

        // 渲染菜单项
        MenuItem* current_item =
            get_child_at_index(menu_ctx.current_menu_id, menu_ctx.page_start_index);

        while (current_item != NULL && rendered_items < menu_ctx.items_in_current_page &&
               rendered_height < default_layout.menu_area_height) {
            bool is_selected = (current_item->id == menu_ctx.current_item_id);
            bool is_editing = is_selected && menu_ctx.is_editing;

            // 使用自定义渲染回调或默认渲染
            if (custom_render_callback != NULL) {
                custom_render_callback(current_item, rendered_items, is_selected, is_editing);
            } else {
                render_menu_item_internal(default_layout.menu_area_top + rendered_height,
                                          current_item, is_selected, is_editing);
            }

            rendered_height += get_menu_item_height(current_item);
            rendered_items++;
            current_item = get_next_sibling(current_item->id);
        }
    }

    // 渲染底部状态栏
    render_footer_internal();
}

// ============================================================================
// 导航和事件处理函数
// ============================================================================

// 上移选择（支持翻页逻辑）
static void handle_move_up(void) {
    uint16_t current_index;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_move_up();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_move_up();
        return;
    }

    if (menu_ctx.is_editing) {
        handle_edit_increase();
        return;
    }

    current_index = get_child_index(menu_ctx.current_menu_id, menu_ctx.current_item_id);
    if (current_index == UINT16_MAX) {
        return;
    }

    if (current_index == menu_ctx.page_start_index) {
        if (menu_ctx.current_page > 1) {
            uint16_t target_page_start_index = 0;
            uint8_t target_items_in_page = 0;

            if (get_page_bounds_by_number(menu_ctx.current_menu_id, menu_ctx.current_page - 1,
                                          &target_page_start_index, &target_items_in_page, NULL) &&
                target_items_in_page > 0) {
                MenuItem* last_item = get_child_at_index(
                    menu_ctx.current_menu_id, target_page_start_index + target_items_in_page - 1);
                if (last_item != NULL) {
                    select_item_and_refresh(last_item->id);
                }
            }
        }
        return;
    }

    MenuItem* prev_item = get_child_at_index(menu_ctx.current_menu_id, current_index - 1);
    if (prev_item != NULL) {
        select_item_and_refresh(prev_item->id);
    }
}

// 下移选择（支持翻页逻辑）
static void handle_move_down(void) {
    uint16_t current_index;
    uint16_t next_page_start_index;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_move_down();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_move_down();
        return;
    }

    if (menu_ctx.is_editing) {
        handle_edit_decrease();
        return;
    }

    current_index = get_child_index(menu_ctx.current_menu_id, menu_ctx.current_item_id);
    if (current_index == UINT16_MAX) {
        return;
    }

    if (menu_ctx.items_in_current_page == 0) {
        return;
    }

    if (current_index < (menu_ctx.page_start_index + menu_ctx.items_in_current_page - 1)) {
        MenuItem* next_item = get_child_at_index(menu_ctx.current_menu_id, current_index + 1);
        if (next_item != NULL) {
            select_item_and_refresh(next_item->id);
        }
        return;
    }

    if (menu_ctx.current_page < menu_ctx.total_pages) {
        next_page_start_index = menu_ctx.page_start_index + menu_ctx.items_in_current_page;
        MenuItem* next_page_first_item =
            get_child_at_index(menu_ctx.current_menu_id, next_page_start_index);
        if (next_page_first_item != NULL) {
            select_item_and_refresh(next_page_first_item->id);
        }
    }
}

// 选择菜单项
static void handle_select_item(void) {
    MenuItem* current = get_menu_item(menu_ctx.current_item_id);

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_select();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_select();
        return;
    }

    if (current == NULL) return;
    switch (current->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        if (alarm_log_is_configured() && current->data.target_menu_id == alarm_log_config.menu_id) {
            alarm_log_open(true);
        } else {
            enter_menu_internal(current->data.target_menu_id, true);
        }
        break;

    case MENU_ITEM_TYPE_ACTION:
        // 执行动作
        if (current->data.action_callback != NULL) {
            current->data.action_callback();
        }
        break;

    case MENU_ITEM_TYPE_TOGGLE:
    case MENU_ITEM_TYPE_MOMENTARY:
    case MENU_ITEM_TYPE_VALUE:
    case MENU_ITEM_TYPE_LIST:
        if (current->type != MENU_ITEM_TYPE_MOMENTARY) {
            begin_edit_session(current);
        }
        break;

    default:
        break;
    }
}

// 返回上一级
static void handle_back(void) {
    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_back();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_back();
        return;
    }

    if (menu_ctx.is_editing) {
        handle_edit_cancel();
        return;
    }

    uint16_t previous_menu_id;
    uint16_t previous_item_id;

    if (pop_history_snapshot(&previous_menu_id, &previous_item_id)) {
        menu_ctx.current_menu_id = previous_menu_id;
        select_item_and_refresh(previous_item_id);
        menu_register_current_page_polling(previous_menu_id);
    }
}

// 编辑模式下增加数值
static void handle_edit_increase(void) {
    MenuItem* item = get_menu_item(menu_ctx.edit_item_id);
    if (item == NULL) return;

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        menu_ctx.edit_pending_value = (menu_ctx.edit_pending_value == 0) ? 1 : 0;
        menu_ctx.need_redraw = true;
        break;

    case MENU_ITEM_TYPE_VALUE: {
        int32_t step = get_current_value_edit_step();
        int32_t new_value = menu_ctx.edit_pending_value + step;

        if (new_value > item->data.max_value) {
            new_value = item->data.max_value;
        }

        if (new_value != menu_ctx.edit_pending_value) {
            menu_ctx.edit_pending_value = new_value;
            menu_ctx.need_redraw = true;
        }
    } break;

    case MENU_ITEM_TYPE_LIST:
        if (item->data.option_count > 0) {
            uint8_t new_selection = (uint8_t)menu_ctx.edit_pending_value + 1;
            if (new_selection < item->data.option_count) {
                menu_ctx.edit_pending_value = new_selection;
                menu_ctx.need_redraw = true;
            }
        }
        break;

    default:
        break;
    }
}

// 编辑模式下减少数值
static void handle_edit_decrease(void) {
    MenuItem* item = get_menu_item(menu_ctx.edit_item_id);
    if (item == NULL) return;

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        menu_ctx.edit_pending_value = (menu_ctx.edit_pending_value == 0) ? 1 : 0;
        menu_ctx.need_redraw = true;
        break;

    case MENU_ITEM_TYPE_VALUE: {
        int32_t step = get_current_value_edit_step();
        int32_t new_value = menu_ctx.edit_pending_value - step;

        if (new_value < item->data.min_value) {
            new_value = item->data.min_value;
        }

        if (new_value != menu_ctx.edit_pending_value) {
            menu_ctx.edit_pending_value = new_value;
            menu_ctx.need_redraw = true;
        }
    } break;

    case MENU_ITEM_TYPE_LIST:
        if (item->data.option_count > 0) {
            uint8_t current_selection = (uint8_t)menu_ctx.edit_pending_value;
            if (current_selection > 0) {
                uint8_t new_selection = current_selection - 1;
                menu_ctx.edit_pending_value = new_selection;
                menu_ctx.need_redraw = true;
            }
        }
        break;

    default:
        break;
    }
}

// 确认编辑
static void handle_edit_confirm(void) {
    MenuItem* item = get_menu_item(menu_ctx.edit_item_id);

    if (item != NULL && !apply_pending_edit_value(item)) {
        menu_ctx.need_redraw = true;
        return;
    }

    menu_ctx.is_editing = false;
    menu_ctx.edit_item_id = 0;
    menu_ctx.edit_original_value = 0;
    menu_ctx.edit_pending_value = 0;
    menu_ctx.edit_value_step_index = 0;
    menu_ctx.edit_flash_tick = 0;
    menu_ctx.edit_flash_inverse = false;
    menu_ctx.current_state = MENU_STATE_BROWSING;
    menu_ctx.need_redraw = true;
}

// 取消编辑
static void handle_edit_cancel(void) {
    menu_ctx.is_editing = false;
    menu_ctx.edit_item_id = 0;
    menu_ctx.edit_original_value = 0;
    menu_ctx.edit_pending_value = 0;
    menu_ctx.edit_value_step_index = 0;
    menu_ctx.edit_flash_tick = 0;
    menu_ctx.edit_flash_inverse = false;
    menu_ctx.current_state = MENU_STATE_BROWSING;
    menu_ctx.need_redraw = true;
}

// ============================================================================
// 公开API函数实现
// ============================================================================

// 菜单系统初始化
void menu_system_init(void) {
    // 初始化菜单上下文
    memset(&menu_ctx, 0, sizeof(MenuContext));
    HashCache_Init();
    PollManager_Init();
    menu_ctx.visible_items = VISIBLE_ITEMS;
    menu_ctx.current_page = 1;
    menu_ctx.total_pages = 1;
    menu_ctx.current_state = MENU_STATE_IDLE;
    menu_ctx.edit_value_step_index = 0;
    menu_ctx.edit_flash_inverse = false;
    menu_ctx.special_view = MENU_SPECIAL_VIEW_NONE;
    menu_ctx.io_current_page = 0;
    menu_ctx.io_total_pages = 0;
    menu_ctx.io_selected_index = 0;
    menu_ctx.io_is_editing = false;
    menu_ctx.io_edit_flash_tick = 0;
    menu_ctx.io_edit_flash_inverse = false;
    momentary_enter_active = false;
    momentary_active_item_id = 0U;
    menu_ctx.need_redraw = true;
}

// 加载菜单配置
bool menu_load_config(const MenuItem* items, uint16_t item_count) {
    if (items == NULL || item_count == 0) {
        return false;
    }

    menu_items = items;
    menu_item_count = item_count;
    menu_register_all_cache_items();
    return true;
}

bool menu_configure_io_monitor(const IOMonitorConfig* config) {
    uint8_t i;

    if (config == NULL) {
        return false;
    }

    io_monitor_config = *config;
    if (!io_monitor_is_configured()) {
        return false;
    }

    for (i = 0; i < io_monitor_config.x_count; i++) {
        HashCache_RegisterBit(io_monitor_config.x_points[i].rs485_addr);
    }

    for (i = 0; i < io_monitor_config.y_count; i++) {
        HashCache_RegisterBit(io_monitor_config.y_points[i].rs485_addr);
    }

    return true;
}

bool menu_configure_alarm_log(const AlarmLogConfig* config) {
    if (config == NULL || config->rs485_addr == MENU_RS485_ADDR_NONE) {
        return false;
    }

    alarm_log_config = *config;
    memset(alarm_log_entries, 0, sizeof(alarm_log_entries));
    alarm_log_head = 0U;
    alarm_log_count = 0U;
    alarm_log_current_page = 0U;
    alarm_log_total_pages = 1U;
    alarm_log_selected_row = 0U;
    alarm_log_last_value = 0U;
    alarm_log_has_last_value = false;

    if (config->rs485_type == MENU_TYPE_VALUE_BIT) {
        HashCache_RegisterBit(config->rs485_addr);
    } else {
        HashCache_RegisterWord(config->rs485_addr);
        if (config->rs485_type == MENU_TYPE_VALUE_DINT ||
            config->rs485_type == MENU_TYPE_VALUE_UINT32) {
            HashCache_RegisterWord((uint16_t)(config->rs485_addr + 1U));
        }
    }

    PollManager_SetAlarmAddress(config->rs485_addr, config->rs485_type);
    return true;
}

void menu_open_io_monitor(void) {
    if (!io_monitor_is_configured()) {
        return;
    }

    menu_ctx.special_view = MENU_SPECIAL_VIEW_IO_MONITOR;
    menu_ctx.io_current_page = 0U;
    menu_ctx.io_selected_index = 0U;
    menu_ctx.io_is_editing = false;
    menu_ctx.io_edit_flash_tick = 0U;
    menu_ctx.io_edit_flash_inverse = false;
    menu_ctx.current_state = MENU_STATE_BROWSING;
    io_monitor_sync_paging();
    io_monitor_register_all_polling();
    menu_ctx.need_redraw = true;
}

void menu_open_alarm_log(void) {
    (void)alarm_log_open(true);
}

// 启动菜单系统
void menu_start(uint16_t initial_menu_id) {
    menu_ctx.current_state = MENU_STATE_BROWSING;
    enter_menu_internal(initial_menu_id, false);
}

// 设置当前语言
void menu_set_language(Language lang) {
    set_language(lang);
    update_page_info();
    menu_ctx.need_redraw = true;
}

// 导航到指定菜单
bool menu_navigate_to(uint16_t menu_id) {
    return enter_menu_internal(menu_id, true);
}

// 返回上一级菜单
bool menu_navigate_back(void) {
    uint16_t previous_menu_id;
    uint16_t previous_item_id;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_close();
        return true;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_close();
        return true;
    }

    if (pop_history_snapshot(&previous_menu_id, &previous_item_id)) {
        menu_ctx.current_menu_id = previous_menu_id;
        select_item_and_refresh(previous_item_id);
        menu_register_current_page_polling(previous_menu_id);
        return true;
    }
    return false;
}

// 返回到根菜单
bool menu_navigate_to_root(void) {
    if (menu_items == NULL || menu_item_count == 0) {
        return false;
    }

    // 假设第一个菜单项是根菜单
    menu_ctx.history_top = 0;
    menu_ctx.special_view = MENU_SPECIAL_VIEW_NONE;
    return enter_menu_internal(menu_items[0].id, false);
}

// 获取当前菜单ID
uint16_t menu_get_current_menu_id(void) {
    return menu_ctx.current_menu_id;
}

// 获取当前选中项ID
uint16_t menu_get_selected_item_id(void) {
    return menu_ctx.current_item_id;
}

// 获取当前菜单状态
MenuState menu_get_current_state(void) {
    return menu_ctx.current_state;
}

// 处理按键事件（支持短按、长按、重复触发）
void menu_handle_key_event(void) {
    KeyEvent event;

    menu_update_momentary_enter_state();

    while (key_control_get_event(&event)) {
        // 根据事件类型处理不同的按键行为
        switch (event.type) {
        case KEY_EVENT_PRESS:
            // 短按事件
            handle_short_press(&event);
            break;

        case KEY_EVENT_LONG_PRESS:
            // 长按事件
            handle_long_press(&event);
            break;

        case KEY_EVENT_REPEAT:
            // 重复触发事件（主要用于长按后的连续操作）
            handle_repeat_event(&event);
            break;

        default:
            // 忽略未知事件类型
            break;
        }
    }
}

void menu_handle_data_updates(const uint16_t* changed_addrs, const uint32_t* changed_values,
                              uint8_t changed_count) {
    uint8_t i;

    if (!alarm_log_is_configured() || changed_addrs == NULL || changed_values == NULL) {
        return;
    }

    for (i = 0U; i < changed_count; i++) {
        uint32_t current_value;

        if (changed_addrs[i] != alarm_log_config.rs485_addr) {
            continue;
        }

        current_value = changed_values[i];
        if (!alarm_log_has_last_value) {
            // if (current_value != 0U) {
            //     alarm_log_append((uint16_t)current_value);
            //     (void)alarm_log_open(true);
            // }
            // alarm_log_last_value = current_value;
            alarm_log_has_last_value = true;
            // continue;
        }

        if (current_value == 0U) {
            if (alarm_log_last_value != 0U) {
                alarm_log_mark_all_read();
                menu_ctx.need_redraw = true;
            }
        } else if (alarm_log_last_value == 0U || current_value != alarm_log_last_value) {
            alarm_log_append((uint16_t)current_value);
            (void)alarm_log_open(true);
        }

        alarm_log_last_value = current_value;
    }
}

// 处理定时器事件
void menu_handle_timer(uint32_t current_tick) {
    update_edit_flash_state(current_tick);

    // 检查是否需要重绘
    if (menu_ctx.need_redraw) {
        render_full();
        menu_ctx.need_redraw = false;
        menu_ctx.last_redraw_time = current_tick;
        bsp_JLXLcdRefreshScreen();  // 刷新屏幕显示
    }
}

// 请求重绘
void menu_request_redraw(void) {
    menu_ctx.need_redraw = true;
}

// 检查是否需要重绘
bool menu_needs_redraw(void) {
    return menu_ctx.need_redraw;
}

// 强制立即重绘
void menu_force_redraw(void) {
    render_full();
    menu_ctx.need_redraw = false;
}

// 设置自定义渲染回调
void menu_set_custom_render_callback(RenderCallback callback) {
    custom_render_callback = callback;
}

// 显示消息框
void menu_show_message(StringID title_id, StringID message_id, uint32_t timeout_ms) {
    // 简化实现：直接显示消息
    menu_ctx.current_state = MENU_STATE_MESSAGE_BOX;
    menu_ctx.need_redraw = true;

    // 注意：实际实现需要存储消息内容和超时时间
}

// 显示确认对话框
void menu_show_confirm(StringID title_id, StringID message_id, ConfirmCallback confirm_cb,
                       ConfirmCallback cancel_cb) {
    // 简化实现
    menu_ctx.current_state = MENU_STATE_CONFIRM_DIALOG;
    menu_ctx.need_redraw = true;

    // 注意：实际实现需要存储回调函数和消息内容
}

// 显示加载指示器
void menu_show_loading(StringID message_id) {
    menu_ctx.current_state = MENU_STATE_LOADING;
    menu_ctx.need_redraw = true;
}

// 隐藏加载指示器
void menu_hide_loading(void) {
    menu_ctx.current_state = MENU_STATE_BROWSING;
    menu_ctx.need_redraw = true;
}

// 获取当前布局配置
const ScreenLayout* menu_get_default_layout(void) {
    return &default_layout;
}

// ============================================================================
// 长按事件处理函数实现
// ============================================================================

// 处理短按事件（支持翻页）
static void handle_short_press(KeyEvent* event) {
    if (event == NULL) return;

    switch (event->key) {
    case FUNKEY_UP:
        handle_move_up();
        break;

    case FUNKEY_DOWN:
        handle_move_down();
        break;

    case FUNKEY_ENTER:
        if (!menu_ctx.is_editing && menu_ctx.special_view == MENU_SPECIAL_VIEW_NONE) {
            MenuItem* current = get_menu_item(menu_ctx.current_item_id);
            if (current != NULL && current->type == MENU_ITEM_TYPE_MOMENTARY) {
                break;
            }
        }

        if (menu_ctx.is_editing) {
            handle_edit_confirm();
        } else {
            handle_select_item();
        }
        break;

    case FUNKEY_ESC:
        handle_back();
        break;

    case FUNKEY_LEFT:
        if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
            io_monitor_page_up();
        } else if (menu_ctx.is_editing) {
            MenuItem* edit_item = get_menu_item(menu_ctx.edit_item_id);
            if (is_value_editing_item(edit_item)) {
                switch_value_edit_step(-1);
            } else {
                handle_edit_decrease();
            }
        } else {
            // 浏览模式下：向前翻页
            handle_page_up();
        }
        break;

    case FUNKEY_RIGHT:
        if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
            io_monitor_page_down();
        } else if (menu_ctx.is_editing) {
            MenuItem* edit_item = get_menu_item(menu_ctx.edit_item_id);
            if (is_value_editing_item(edit_item)) {
                switch_value_edit_step(1);
            } else {
                handle_edit_increase();
            }
        } else {
            // 浏览模式下：向后翻页
            handle_page_down();
        }
        break;

    default:
        break;
    }
}

// 处理长按事件（特殊功能：快速导航/快速调整）
static void handle_long_press(KeyEvent* event) {
    if (event == NULL) return;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR ||
        menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        return;
    }

    switch (event->key) {
    case FUNKEY_UP:
        if (!menu_ctx.is_editing) {
            MenuItem* first_item = get_first_child(menu_ctx.current_menu_id);
            if (first_item != NULL) {
                select_item_and_refresh(first_item->id);
            }
        }
        break;

    case FUNKEY_DOWN:
        if (!menu_ctx.is_editing && menu_ctx.total_pages > 0) {
            uint16_t page_start_index = 0;
            uint8_t items_in_page = 0;

            if (get_page_bounds_by_number(menu_ctx.current_menu_id, menu_ctx.total_pages,
                                          &page_start_index, &items_in_page, NULL) &&
                items_in_page > 0) {
                MenuItem* last_item = get_child_at_index(menu_ctx.current_menu_id,
                                                         page_start_index + items_in_page - 1);
                if (last_item != NULL) {
                    select_item_and_refresh(last_item->id);
                }
            }
        }
        break;

    case FUNKEY_LEFT:
        break;

    case FUNKEY_RIGHT:
        break;

    case FUNKEY_ENTER:
        break;

    case FUNKEY_ESC:
        if (!menu_ctx.is_editing) {
            menu_navigate_to_root();
        }
        break;

    default:
        break;
    }
}

// 处理重复触发事件（主要用于长按后的连续操作）
static void handle_repeat_event(KeyEvent* event) {
    if (event == NULL) return;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        switch (event->key) {
        case FUNKEY_UP:
            alarm_log_move_up();
            break;

        case FUNKEY_DOWN:
            alarm_log_move_down();
            break;

        default:
            break;
        }
        return;
    }

    switch (event->key) {
    case FUNKEY_UP:
        handle_move_up();
        break;

    case FUNKEY_DOWN:
        handle_move_down();
        break;

    case FUNKEY_LEFT:
        break;

    case FUNKEY_RIGHT:
        break;

    default:
        break;
    }
}

// ============================================================================
// 翻页相关函数实现
// ============================================================================

// 更新页面信息：根据菜单项实际像素高度计算总页数和当前页信息
static void update_page_info(void) {
    uint16_t child_count;
    uint16_t selected_index;
    uint16_t current_index = 0;
    uint8_t current_page = 1;
    bool found_page = false;

    if (menu_items == NULL || menu_item_count == 0) {
        menu_ctx.total_pages = 1;
        menu_ctx.current_page = 1;
        menu_ctx.items_in_current_page = 0;
        menu_ctx.page_start_index = 0;
        menu_ctx.scroll_position = 0;
        menu_ctx.visible_items = 0;
        return;
    }

    // 获取当前菜单的子项数量
    child_count = count_children(menu_ctx.current_menu_id);
    if (child_count == 0) {
        menu_ctx.total_pages = 1;
        menu_ctx.current_page = 1;
        menu_ctx.items_in_current_page = 0;
        menu_ctx.page_start_index = 0;
        menu_ctx.scroll_position = 0;
        menu_ctx.visible_items = 0;
        return;
    }

    // 找到选中项在子项中的索引，如果当前选中项失效则回退到第一页第一项
    selected_index = get_child_index(menu_ctx.current_menu_id, menu_ctx.current_item_id);
    if (selected_index == UINT16_MAX) {
        MenuItem* first_child = get_first_child(menu_ctx.current_menu_id);
        selected_index = 0;
        if (first_child != NULL) {
            menu_ctx.current_item_id = first_child->id;
        }
    }

    menu_ctx.total_pages = 0;
    while (current_index < child_count) {
        uint8_t page_items = build_page_from_index(menu_ctx.current_menu_id, current_index, NULL);

        if (page_items == 0) {
            page_items = 1;
        }

        menu_ctx.total_pages++;

        if (!found_page && selected_index >= current_index &&
            selected_index < (current_index + page_items)) {
            menu_ctx.current_page = current_page;
            menu_ctx.page_start_index = current_index;
            menu_ctx.items_in_current_page = page_items;
            found_page = true;
        }

        current_index += page_items;
        current_page++;
    }

    if (menu_ctx.total_pages == 0) {
        menu_ctx.total_pages = 1;
    }

    if (!found_page) {
        menu_ctx.current_page = 1;
        menu_ctx.page_start_index = 0;
        menu_ctx.items_in_current_page = build_page_from_index(menu_ctx.current_menu_id, 0, NULL);
    }

    // 更新滚动位置
    menu_ctx.scroll_position = menu_ctx.page_start_index;
    menu_ctx.visible_items = menu_ctx.items_in_current_page;
}

// 向前翻页（左键）
static void handle_page_up(void) {
    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_page_up();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_page_up();
        return;
    }

    if (menu_ctx.is_editing) {
        // 编辑模式下保持原有功能
        handle_edit_decrease();
        return;
    }

    navigate_relative_page(-1);
}

// 向后翻页（右键）
static void handle_page_down(void) {
    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        alarm_log_page_down();
        return;
    }

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        io_monitor_page_down();
        return;
    }

    if (menu_ctx.is_editing) {
        // 编辑模式下保持原有功能
        handle_edit_increase();
        return;
    }

    navigate_relative_page(1);
}

// 渲染页码信息（在底部状态栏显示）
static void render_page_info(void) {
    // 只有在多页时才显示页码信息
    uint8_t current_page = menu_ctx.current_page;
    uint8_t total_pages = menu_ctx.total_pages;

    if (menu_ctx.special_view == MENU_SPECIAL_VIEW_IO_MONITOR) {
        current_page = (uint8_t)(menu_ctx.io_current_page + 1U);
        total_pages = menu_ctx.io_total_pages;
    } else if (menu_ctx.special_view == MENU_SPECIAL_VIEW_ALARM_LOG) {
        current_page = (uint8_t)(alarm_log_current_page + 1U);
        total_pages = alarm_log_total_pages;
    }

    if (total_pages > 1) {
        uint8_t y_pos = JLXLCD_H - default_layout.footer_height;
        char page_str[16];
        uint16_t page_width;
        uint16_t x_pos;

        snprintf(page_str, sizeof(page_str), "(%d/%d)", current_page, total_pages);

        page_width = menu_calculate_text_width(page_str);
        x_pos = (page_width + 2 < JLXLCD_W) ? (JLXLCD_W - page_width - 2) : 0;

        // 在底部状态栏右侧显示页码
        JLX_ShowStringAnyRow(x_pos, y_pos + 4, page_str, default_layout.font_size, 0);
    }
}
