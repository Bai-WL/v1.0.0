// menu_system.c
#include "menu_system.h"

#include <stdio.h>
#include <string.h>

#include "bsp_gd32f303xx_jlx192128g.h"
#include "jlx_display_ext.h"

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

// 计算可视项数量（考虑多行文本）
#define VISIBLE_ITEMS (default_layout.menu_area_height / default_layout.item_height)  // 5项

// 事件队列大小
#define EVENT_QUEUE_SIZE 16

// ============================================================================
// 内部全局变量
// ============================================================================

// 菜单配置
static const MenuItem* menu_items = NULL;
static uint16_t menu_item_count = 0;

// 菜单上下文
static MenuContext menu_ctx = {.current_menu_id = 0,
                               .current_item_id = 0,
                               .scroll_position = 0,
                               .visible_items = 5,  // VISIBLE_ITEMS 计算结果为 88/16 = 5
                               .is_editing = false,
                               .edit_item_id = 0,
                               .history_top = 0,
                               .need_redraw = true,
                               .last_redraw_time = 0,
                               .current_state = MENU_STATE_IDLE};

// 按键映射配置（默认映射）
static FunKeyMapping key_mapping[6] = {
    FUNKEY_UP,     // F1
    FUNKEY_DOWN,   // F2
    FUNKEY_LEFT,   // F3
    FUNKEY_RIGHT,  // F4
    FUNKEY_ENTER,  // F5
    FUNKEY_ESC     // F6
};

// 默认按键配置
static KeyConfig default_key_config = {
    .debounce_time = 20,      // 20ms消抖
    .long_press_time = 1000,  // 1秒长按
    .repeat_delay = 500,      // 长按500ms后开始重复
    .repeat_interval = 100    // 每100ms重复一次
};

// 自定义渲染回调
static RenderCallback custom_render_callback = NULL;

// ============================================================================
// 内部辅助函数声明
// ============================================================================

static MenuItem* get_menu_item(uint16_t id);
static MenuItem* get_first_child(uint16_t parent_id);
static MenuItem* get_next_sibling(uint16_t current_id);
static uint8_t count_children(uint16_t parent_id);
static void render_menu_item_internal(uint8_t index, MenuItem* item, bool is_selected, bool is_editing);
static void render_header_internal(void);
static void render_footer_internal(void);
static void handle_move_up(void);
static void handle_move_down(void);
static void handle_select_item(void);
static void handle_back(void);
static void handle_edit_increase(void);
static void handle_edit_decrease(void);
static void handle_edit_confirm(void);
static void handle_edit_cancel(void);

// ============================================================================
// 文本处理函数实现
// ============================================================================

// 文本宽度计算辅助函数
uint16_t menu_calculate_text_width(const char* text) {
    uint16_t width = 0;
    const char* p = text;

    while (*p) {
        // 简单判断：ASCII字符（英文）用英文宽度，其他用中文宽度
        if ((uint8_t)*p < 128) {
            width += default_layout.en_char_width;
        } else {
            width += default_layout.cn_char_width;
        }
        p++;
    }
    return width;
}

// 自动换行函数：将文本分割为多行，确保每行不超过最大宽度
// 返回实际使用的行数（1或2）
uint8_t menu_wrap_text_lines(char line1[32], char line2[32], const char* text, uint16_t max_width) {
    uint16_t current_width = 0;
    uint16_t char_count = 0;
    uint8_t line_index = 0;
    uint16_t last_space_pos = 0;  // 最后一个空格位置
    uint16_t last_space_width = 0;
    const char* src = text;
    char* current_line = line1;

    while (*src && line_index < 2) {
        uint16_t char_width = ((uint8_t)*src < 128) ? default_layout.en_char_width : default_layout.cn_char_width;

        // 检查添加当前字符后是否会超出宽度
        if (current_width + char_width > max_width) {
            // 如果已经超过一行，需要换行
            if (line_index == 0) {
                // 第一行结束
                if (last_space_pos > 0) {
                    // 在最后一个空格处换行
                    current_line[last_space_pos] = '\0';
                    src = text + last_space_pos + 1;  // 跳过空格
                } else {
                    // 没有空格，在当前字符处截断
                    current_line[char_count] = '\0';
                }

                // 准备第二行
                line_index = 1;
                current_line = line2;
                char_count = 0;
                current_width = 0;
                last_space_pos = 0;
                last_space_width = 0;

                // 重新开始处理（src已更新）
                continue;
            } else {
                // 已经是第二行，无法再换行，截断并添加省略号
                if (char_count > 0) {
                    // 回退一个字符，为省略号留出空间
                    char_count--;
                    current_width -= ((uint8_t)*(src - 1) < 128) ? default_layout.en_char_width : default_layout.cn_char_width;

                    // 添加省略号
                    if (current_width + default_layout.en_char_width * 3 <= max_width) {
                        current_line[char_count++] = '.';
                        current_line[char_count++] = '.';
                        current_line[char_count++] = '.';
                    }
                }
                break;
            }
        }

        // 记录空格位置，用于在单词边界换行
        if (*src == ' ' || *src == '\t') {
            last_space_pos = char_count;
            last_space_width = current_width;
        }

        current_line[char_count++] = *src;
        current_width += char_width;
        src++;
    }

    // 设置当前行结束
    if (line_index == 0) {
        current_line[char_count] = '\0';
        line2[0] = '\0';  // 第二行为空
        return 1;
    } else {
        current_line[char_count] = '\0';
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
static uint8_t count_children(uint16_t parent_id) {
    uint8_t count = 0;
    MenuItem* child = get_first_child(parent_id);

    while (child != NULL) {
        count++;
        child = get_next_sibling(child->id);
    }

    return count;
}

// ============================================================================
// 渲染函数实现
// ============================================================================

// 渲染菜单项（支持多行文本）
static void render_menu_item_internal(uint8_t index, MenuItem* item, bool is_selected, bool is_editing) {
    if (item == NULL) return;

    uint8_t y_pos = default_layout.menu_area_top + (index * default_layout.item_height);

    // 清除区域（清除整个菜单项区域）
    JLX_ClearRectPixel(0, JLXLCD_W, y_pos, default_layout.item_height, 0);

    // 绘制选择指示器
    if (is_selected) {
        if (is_editing) {
            bsp_JLXLcdShowString_Any_row(2, y_pos, ">", default_layout.font_size, 1);
        } else {
            bsp_JLXLcdShowString_Any_row(2, y_pos, ">", default_layout.font_size, 1);
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
    uint16_t max_text_width = JLXLCD_W - text_x - 16;  // 默认留出16像素右边距

    // 根据菜单项类型调整文本宽度，为附加信息预留空间
    switch (item->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        max_text_width -= 20;  // ">" 指示器
        break;

    case MENU_ITEM_TYPE_TOGGLE:
        max_text_width -= 32;  // 为开关值留出空间
        break;

    case MENU_ITEM_TYPE_VALUE:
        max_text_width -= 48;  // 为数值留出空间
        break;

    case MENU_ITEM_TYPE_LIST:
        max_text_width -= 64;  // 为选项文本留出空间
        break;

    default:
        break;
    }

    // 计算文本布局（自动换行）
    menu_calculate_text_layout(&text_layout, text, max_text_width);

    // 绘制第一行文本
    if (text_layout.line_count > 0) {
        bsp_JLXLcdShowString_Any_row(text_x, y_pos, text_layout.line1, default_layout.font_size, 1);
    }

    // 绘制第二行文本（如果有）
    if (text_layout.line_count > 1) {
        bsp_JLXLcdShowString_Any_row(text_x, y_pos + default_layout.line_height, text_layout.line2, default_layout.font_size, 1);
    }

    // 根据类型绘制附加信息
    switch (item->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        // 子菜单箭头，根据行数调整位置
        if (text_layout.line_count > 1) {
            bsp_JLXLcdShowString_Any_row(JLXLCD_W - 16, y_pos + default_layout.line_height, ">", default_layout.font_size, 1);
        } else {
            bsp_JLXLcdShowString_Any_row(JLXLCD_W - 16, y_pos, ">", default_layout.font_size, 1);
        }
        break;

    case MENU_ITEM_TYPE_TOGGLE: {
        const char* toggle_text = NULL;
        if (item->data.toggle_value != NULL) {
            toggle_text = *(item->data.toggle_value) ? get_string(STR_YES) : get_string(STR_NO);
        }
        if (toggle_text != NULL) {
            // 根据行数调整位置
            if (text_layout.line_count > 1) {
                bsp_JLXLcdShowString_Any_row(JLXLCD_W - 32, y_pos + default_layout.line_height, toggle_text, default_layout.font_size, 1);
            } else {
                bsp_JLXLcdShowString_Any_row(JLXLCD_W - 32, y_pos, toggle_text, default_layout.font_size, 1);
            }
        }
    } break;

    case MENU_ITEM_TYPE_VALUE: {
        char value_str[16];
        if (item->data.value_ptr != NULL) {
            snprintf(value_str, sizeof(value_str), "%d", *(item->data.value_ptr));
            // 根据行数调整位置
            if (text_layout.line_count > 1) {
                bsp_JLXLcdShowString_Any_row(JLXLCD_W - 48, y_pos + default_layout.line_height, value_str, default_layout.font_size, 1);

                if (is_selected && is_editing) {
                    // 编辑模式下高亮显示
                    bsp_JLXLcdShowUnderline(JLXLCD_W - 48, y_pos + default_layout.line_height + 14, 32);
                }
            } else {
                bsp_JLXLcdShowString_Any_row(JLXLCD_W - 48, y_pos, value_str, default_layout.font_size, 1);

                if (is_selected && is_editing) {
                    // 编辑模式下高亮显示
                    bsp_JLXLcdShowUnderline(JLXLCD_W - 48, y_pos + 14, 32);
                }
            }
        }
    } break;

    case MENU_ITEM_TYPE_LIST: {
        if (item->data.selection_ptr != NULL && item->data.option_ids != NULL) {
            uint8_t selection = *(item->data.selection_ptr);
            if (selection < item->data.option_count) {
                const char* selected_text = get_string(item->data.option_ids[selection]);
                if (selected_text != NULL) {
                    // 根据行数调整位置
                    if (text_layout.line_count > 1) {
                        bsp_JLXLcdShowString_Any_row(JLXLCD_W - 64, y_pos + default_layout.line_height, selected_text, default_layout.font_size, 1);

                        if (is_selected && is_editing) {
                            bsp_JLXLcdShowString_Any_row(JLXLCD_W - 16, y_pos + default_layout.line_height, "v", default_layout.font_size, 1);
                        }
                    } else {
                        bsp_JLXLcdShowString_Any_row(JLXLCD_W - 64, y_pos, selected_text, default_layout.font_size, 1);

                        if (is_selected && is_editing) {
                            bsp_JLXLcdShowString_Any_row(JLXLCD_W - 16, y_pos, "v", default_layout.font_size, 1);
                        }
                    }
                }
            }
        }
    } break;

    default:
        break;
    }
}

// 渲染标题栏
static void render_header_internal(void) {
    // 清除标题栏区域
    JLX_ClearRectPixel(0, JLXLCD_W, 0, default_layout.header_height, 0);

    // 获取当前菜单标题
    MenuItem* current_menu = get_menu_item(menu_ctx.current_menu_id);
    if (current_menu != NULL) {
        const char* title = get_string(current_menu->text_id);
        if (title != NULL) {
            bsp_JLXLcdShowString_Any_row(2, 2, title, default_layout.font_size, 0);
            bsp_JLXLcdRefreshScreen();  // 刷新屏幕显示
        }
    }

    // 注意：电池状态和时间显示需要根据实际情况实现
    // render_battery_status();
    // render_current_time();
}

// 渲染底部状态栏
static void render_footer_internal(void) {
    uint8_t y_pos = JLXLCD_H - default_layout.footer_height;

    // 清除底部状态栏区域
    JLX_ClearRectPixel(0, JLXLCD_W, y_pos, default_layout.footer_height, 0);

    // 绘制帮助文本
    MenuItem* current_item = get_menu_item(menu_ctx.current_item_id);
    if (current_item && current_item->help_id != STR_NONE) {
        const char* help_text = get_string(current_item->help_id);
        if (help_text != NULL) {
            bsp_JLXLcdShowString_Any_row(2, y_pos + 4, help_text, default_layout.font_size, 1);
        }
    }

        // 绘制操作提示
    if (menu_ctx.is_editing) {
        bsp_JLXLcdShowString_Any_row(2, y_pos + 4, "上/下:调整 左/右:快速调整", default_layout.font_size, 1);
        bsp_JLXLcdShowString_Any_row(JLXLCD_W - 64, y_pos + 4, "确认:√ 取消:X", default_layout.font_size, 1);
    } else {
        bsp_JLXLcdShowString_Any_row(2, y_pos + 4, "上/下:选择 左/右:翻页", default_layout.font_size, 1);
        bsp_JLXLcdShowString_Any_row(JLXLCD_W - 48, y_pos + 4, "确认:→ 返回:←", default_layout.font_size, 1);
    }
}

// 完整渲染函数
static void render_full(void) {
    // 渲染标题栏
    render_header_internal();

    // 渲染菜单项
    MenuItem* current_item = get_first_child(menu_ctx.current_menu_id);
    uint8_t item_index = 0;
    uint8_t display_index = 0;

    while (current_item != NULL && display_index < VISIBLE_ITEMS) {
        // 跳过滚出屏幕的项
        if (item_index >= menu_ctx.scroll_position) {
            bool is_selected = (current_item->id == menu_ctx.current_item_id);
            bool is_editing = is_selected && menu_ctx.is_editing;

            // 使用自定义渲染回调或默认渲染
            if (custom_render_callback != NULL) {
                custom_render_callback(current_item, display_index, is_selected, is_editing);
            } else {
                render_menu_item_internal(display_index, current_item, is_selected, is_editing);
            }

            display_index++;
        }

        current_item = get_next_sibling(current_item->id);
        item_index++;
    }

    // 渲染底部状态栏
    render_footer_internal();
}

// ============================================================================
// 导航和事件处理函数
// ============================================================================

// 上移选择
static void handle_move_up(void) {
    if (menu_ctx.is_editing) {
        handle_edit_increase();
        return;
    }

    MenuItem* current = get_menu_item(menu_ctx.current_item_id);
    if (current == NULL) return;

    // 查找前一个兄弟项
    MenuItem* prev = get_first_child(menu_ctx.current_menu_id);
    MenuItem* candidate = NULL;

    while (prev != NULL) {
        if (prev->id == current->id) {
            if (candidate != NULL) {
                menu_ctx.current_item_id = candidate->id;
                menu_ctx.need_redraw = true;

                // 调整滚动位置
                if (menu_ctx.scroll_position > 0) {
                    menu_ctx.scroll_position--;
                }
            }
            break;
        }
        candidate = prev;
        prev = get_next_sibling(prev->id);
    }
}

// 下移选择
static void handle_move_down(void) {
    if (menu_ctx.is_editing) {
        handle_edit_decrease();
        return;
    }

    MenuItem* current = get_menu_item(menu_ctx.current_item_id);
    if (current == NULL) return;

    // 查找下一个兄弟项
    MenuItem* next = get_next_sibling(current->id);
    if (next != NULL) {
        menu_ctx.current_item_id = next->id;
        menu_ctx.need_redraw = true;

        // 调整滚动位置
        uint8_t visible_items = VISIBLE_ITEMS;
        uint8_t item_index = 0;
        MenuItem* item = get_first_child(menu_ctx.current_menu_id);

        while (item != NULL && item->id != next->id) {
            item_index++;
            item = get_next_sibling(item->id);
        }

        if (item_index >= menu_ctx.scroll_position + visible_items) {
            menu_ctx.scroll_position = item_index - visible_items + 1;
        }
    }
}

// 选择菜单项
static void handle_select_item(void) {
    MenuItem* current = get_menu_item(menu_ctx.current_item_id);
    if (current == NULL) return;
    MenuItem* first_child = NULL;
    switch (current->type) {
    case MENU_ITEM_TYPE_SUBMENU:
        // 进入子菜单
        menu_ctx.history_stack[menu_ctx.history_top++] = menu_ctx.current_menu_id;
        if (menu_ctx.history_top >= 8) menu_ctx.history_top = 7;

        menu_ctx.current_menu_id = current->data.target_menu_id;
        menu_ctx.scroll_position = 0;

        // 设置第一个子项为当前项
        first_child = get_first_child(menu_ctx.current_menu_id);
        if (first_child != NULL) {
            menu_ctx.current_item_id = first_child->id;
        }
        menu_ctx.need_redraw = true;
        break;

    case MENU_ITEM_TYPE_ACTION:
        // 执行动作
        if (current->data.action_callback != NULL) {
            current->data.action_callback();
        }
        break;

    case MENU_ITEM_TYPE_TOGGLE:
    case MENU_ITEM_TYPE_VALUE:
    case MENU_ITEM_TYPE_LIST:
        // 进入编辑模式
        menu_ctx.is_editing = true;
        menu_ctx.edit_item_id = current->id;
        menu_ctx.current_state = MENU_STATE_EDITING;
        menu_ctx.need_redraw = true;
        break;

    default:
        break;
    }
}

// 返回上一级
static void handle_back(void) {
    if (menu_ctx.is_editing) {
        handle_edit_cancel();
        return;
    }

    if (menu_ctx.history_top > 0) {
        menu_ctx.history_top--;
        menu_ctx.current_menu_id = menu_ctx.history_stack[menu_ctx.history_top];
        menu_ctx.scroll_position = 0;

        // 设置第一个子项为当前项
        MenuItem* first_child = get_first_child(menu_ctx.current_menu_id);
        if (first_child != NULL) {
            menu_ctx.current_item_id = first_child->id;
        }
        menu_ctx.need_redraw = true;
    }
}

// 编辑模式下增加数值
static void handle_edit_increase(void) {
    MenuItem* item = get_menu_item(menu_ctx.edit_item_id);
    if (item == NULL) return;

    switch (item->type) {
    case MENU_ITEM_TYPE_TOGGLE:
        if (item->data.toggle_value != NULL) {
            *(item->data.toggle_value) = !*(item->data.toggle_value);
            if (item->data.toggle_changed != NULL) {
                item->data.toggle_changed(*(item->data.toggle_value));
            }
            menu_ctx.need_redraw = true;
        }
        break;

    case MENU_ITEM_TYPE_VALUE:
        if (item->data.value_ptr != NULL) {
            int32_t new_value = *(item->data.value_ptr) + item->data.step_value;
            if (new_value <= item->data.max_value) {
                *(item->data.value_ptr) = new_value;
                if (item->data.value_changed != NULL) {
                    item->data.value_changed(new_value);
                }
                menu_ctx.need_redraw = true;
            }
        }
        break;

    case MENU_ITEM_TYPE_LIST:
        if (item->data.selection_ptr != NULL && item->data.option_count > 0) {
            uint8_t new_selection = *(item->data.selection_ptr) + 1;
            if (new_selection < item->data.option_count) {
                *(item->data.selection_ptr) = new_selection;
                if (item->data.selection_changed != NULL) {
                    item->data.selection_changed(new_selection);
                }
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
        if (item->data.toggle_value != NULL) {
            *(item->data.toggle_value) = !*(item->data.toggle_value);
            if (item->data.toggle_changed != NULL) {
                item->data.toggle_changed(*(item->data.toggle_value));
            }
            menu_ctx.need_redraw = true;
        }
        break;

    case MENU_ITEM_TYPE_VALUE:
        if (item->data.value_ptr != NULL) {
            int32_t new_value = *(item->data.value_ptr) - item->data.step_value;
            if (new_value >= item->data.min_value) {
                *(item->data.value_ptr) = new_value;
                if (item->data.value_changed != NULL) {
                    item->data.value_changed(new_value);
                }
                menu_ctx.need_redraw = true;
            }
        }
        break;

    case MENU_ITEM_TYPE_LIST:
        if (item->data.selection_ptr != NULL && item->data.option_count > 0) {
            uint8_t current_selection = *(item->data.selection_ptr);
            if (current_selection > 0) {
                uint8_t new_selection = current_selection - 1;
                *(item->data.selection_ptr) = new_selection;
                if (item->data.selection_changed != NULL) {
                    item->data.selection_changed(new_selection);
                }
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
    menu_ctx.is_editing = false;
    menu_ctx.edit_item_id = 0;
    menu_ctx.current_state = MENU_STATE_BROWSING;
    menu_ctx.need_redraw = true;
}

// 取消编辑
static void handle_edit_cancel(void) {
    menu_ctx.is_editing = false;
    menu_ctx.edit_item_id = 0;
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
    menu_ctx.visible_items = VISIBLE_ITEMS;
    menu_ctx.current_state = MENU_STATE_IDLE;
    menu_ctx.need_redraw = true;

    // 初始化显示驱动
    bsp_JLXLcdInit();
}

// 加载菜单配置
bool menu_load_config(const MenuItem* items, uint16_t item_count) {
    if (items == NULL || item_count == 0) {
        return false;
    }

    menu_items = items;
    menu_item_count = item_count;
    return true;
}

// 启动菜单系统
void menu_start(uint16_t initial_menu_id) {
    menu_ctx.current_menu_id = initial_menu_id;
    menu_ctx.current_state = MENU_STATE_BROWSING;

    // 设置第一个子项为当前项
    MenuItem* first_child = get_first_child(initial_menu_id);
    if (first_child != NULL) {
        menu_ctx.current_item_id = first_child->id;
    }

    menu_ctx.need_redraw = true;
}

// 设置当前语言
void menu_set_language(Language lang) {
    set_language(lang);
    menu_ctx.need_redraw = true;
}

// 导航到指定菜单
bool menu_navigate_to(uint16_t menu_id) {
    MenuItem* target = get_menu_item(menu_id);
    if (target == NULL || target->type != MENU_ITEM_TYPE_SUBMENU) {
        return false;
    }

    menu_ctx.history_stack[menu_ctx.history_top++] = menu_ctx.current_menu_id;
    if (menu_ctx.history_top >= 8) menu_ctx.history_top = 7;

    menu_ctx.current_menu_id = menu_id;
    menu_ctx.scroll_position = 0;

    // 设置第一个子项为当前项
    MenuItem* first_child = get_first_child(menu_id);
    if (first_child != NULL) {
        menu_ctx.current_item_id = first_child->id;
    }

    menu_ctx.need_redraw = true;
    return true;
}

// 返回上一级菜单
bool menu_navigate_back(void) {
    if (menu_ctx.history_top > 0) {
        menu_ctx.history_top--;
        menu_ctx.current_menu_id = menu_ctx.history_stack[menu_ctx.history_top];
        menu_ctx.scroll_position = 0;

        // 设置第一个子项为当前项
        MenuItem* first_child = get_first_child(menu_ctx.current_menu_id);
        if (first_child != NULL) {
            menu_ctx.current_item_id = first_child->id;
        }

        menu_ctx.need_redraw = true;
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
    menu_ctx.current_menu_id = menu_items[0].id;
    menu_ctx.scroll_position = 0;
    menu_ctx.history_top = 0;

    // 设置第一个子项为当前项
    MenuItem* first_child = get_first_child(menu_ctx.current_menu_id);
    if (first_child != NULL) {
        menu_ctx.current_item_id = first_child->id;
    }

    menu_ctx.need_redraw = true;
    return true;
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

// 处理按键事件
void menu_handle_key_event(FunKeyMapping key, KeyEventType event_type) {
    if (event_type != KEY_EVENT_PRESS) {
        return;  // 目前只处理短按事件
    }

    switch (key) {
    case FUNKEY_UP:
        handle_move_up();
        break;

    case FUNKEY_DOWN:
        handle_move_down();
        break;

    case FUNKEY_ENTER:
        handle_select_item();
        break;

    case FUNKEY_ESC:
        handle_back();
        break;

    case FUNKEY_LEFT:
        if (menu_ctx.is_editing) {
            handle_edit_decrease();
        }
        break;

    case FUNKEY_RIGHT:
        if (menu_ctx.is_editing) {
            handle_edit_increase();
        }
        break;

    default:
        break;
    }
}

// 处理定时器事件
void menu_handle_timer(uint32_t current_tick) {
    // 检查是否需要重绘
    if (menu_ctx.need_redraw) {
        render_full();
        menu_ctx.need_redraw = false;
        menu_ctx.last_redraw_time = current_tick;
    }
}

// 设置按键配置
void menu_set_key_config(const KeyConfig* config) {
    if (config != NULL) {
        default_key_config = *config;
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
void menu_show_confirm(StringID title_id, StringID message_id, ConfirmCallback confirm_cb, ConfirmCallback cancel_cb) {
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

// 设置自定义按键映射
void menu_set_key_mapping(const FunKeyMapping* mapping) {
    if (mapping != NULL) {
        for (int i = 0; i < 6; i++) {
            key_mapping[i] = mapping[i];
        }
    }
}

// 获取当前布局配置
const ScreenLayout* menu_get_default_layout(void) {
    return &default_layout;
}
