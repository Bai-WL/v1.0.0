/**
 * @file jlx_display_ext.c
 * @brief 扩展JL192128 LCD显示函数库实现
 * @version 1.0
 * @date 2026-04-16
 *
 * @copyright Copyright (c) 2026
 */

#include "jlx_display_ext.h"

#include <string.h>

#include "bsp_font.h"
#include "bsp_gd32f303xx_jlx192128g.h"

// 帧缓冲区指针
static char* g_frame_buffer = NULL;

// 屏幕尺寸
#define JLX_PAGE_COUNT 16  // 128像素 / 8 = 16页

/**
 * @brief 初始化显示扩展库
 */
void JLX_DisplayExt_Init(char* frame_buffer) {
    g_frame_buffer = frame_buffer;
}

/**
 * @brief 清除矩形区域（像素级高度）
 * @note 该函数操作帧缓冲区的单个像素位，支持任意像素高度
 */
void JLX_ClearRectPixel(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    uint16_t i, j;
    uint16_t start_page, end_page;
    uint8_t start_mask, end_mask;
    uint8_t full_page_mask = 0xFF;
    uint8_t clear_mask;

    // 参数边界检查
    if (x >= JLXLCD_EXT_W || y >= JLXLCD_EXT_H || width == 0 || height == 0) return;

    // 计算矩形边界
    uint16_t x_end = x + width;
    uint16_t y_end = y + height;

    if (x_end > JLXLCD_EXT_W) x_end = JLXLCD_EXT_W;
    if (y_end > JLXLCD_EXT_H) y_end = JLXLCD_EXT_H;

    // 计算起始页和结束页
    start_page = y / 8;
    end_page = (y_end - 1) / 8;

    // 计算起始和结束掩码
    uint8_t start_bit = y % 8;
    uint8_t end_bit = (y_end - 1) % 8;

    start_mask = 0xFF >> start_bit;
    end_mask = 0xFF << (7 - end_bit);

    // 遍历所有涉及的页
    for (i = start_page; i <= end_page && i < JLX_PAGE_COUNT; i++) {
        // 确定当前页的掩码
        if (i == start_page && i == end_page) {
            // 矩形只覆盖一页的部分
            clear_mask = start_mask & end_mask;
        } else if (i == start_page) {
            // 起始页，只清除下半部分
            clear_mask = start_mask;
        } else if (i == end_page) {
            // 结束页，只清除上半部分
            clear_mask = end_mask;
        } else {
            // 中间页，清除整页
            clear_mask = full_page_mask;
        }

        // 遍历列
        for (j = x; j < x_end; j++) {
            uint32_t buffer_index = i * JLXLCD_EXT_W + j;

            if (buffer_index >= JLXLCD_EXT_W * JLX_PAGE_COUNT) continue;

            if (color == 0x00)  // 白色 - 清除像素
            {
                g_frame_buffer[buffer_index] &= ~clear_mask;
            } else if (color == 0x01)  // 黑色 - 设置像素
            {
                g_frame_buffer[buffer_index] |= clear_mask;
            } else  // 其他颜色值
            {
                g_frame_buffer[buffer_index] = color;
            }
        }
    }
}

/**
 * @brief 显示一个13x16像素的汉字
 */
static void show_chn_char_13x16(uint16_t x, uint16_t y, const char* _pcStr, uint8_t mode) {
    
    uint32_t index = ulCHNHashMapGetIdx(*((uint16_t*)_pcStr));

    uint16_t chn_count = usCNCntGet_13x16();

    if (index >= chn_count) return;

    uint16_t start_page = y / 8;
    uint8_t start_offset = y % 8;
    uint8_t i, j;

    for (i = 0; i < 2; i++)  // 2行，每行8像素
    {
        for (j = 0; j < 13; j++)  // 13列
        {
            uint8_t pixel_data = Chinese_text_13x16[index]._ucCHN_Code[i * 13 + j];

            if (mode == 1)  // 反色
                pixel_data = ~pixel_data;

            // 计算缓冲区位置
            uint32_t buffer_index = (start_page + i) * JLXLCD_EXT_W + x + j;

            if (buffer_index >= JLXLCD_EXT_W * JLX_PAGE_COUNT) continue;

            if (start_offset) {
                // 跨页处理
                g_frame_buffer[buffer_index] |= pixel_data >> start_offset;

                if (start_page + i + 1 < JLX_PAGE_COUNT) {
                    uint32_t next_buffer_index = (start_page + i + 1) * JLXLCD_EXT_W + x + j;
                    g_frame_buffer[next_buffer_index] |=
                        (uint8_t)(pixel_data << (8 - start_offset));
                }
            } else {
                g_frame_buffer[buffer_index] |= pixel_data;
            }
        }
    }
}

/**
 * @brief 显示一个7x16像素的英文字符
 */
static void show_eng_char_7x16(uint16_t x, uint16_t y, uint8_t ascii_char, uint8_t mode) {
    if (ascii_char < ' ' || ascii_char > '~') return;

    uint8_t ascii_index = ascii_char - ' ';
    uint16_t start_page = y / 8;
    uint8_t start_offset = y % 8;
    uint8_t i, j;

    for (i = 0; i < 2; i++)  // 2行，每行8像素
    {
        for (j = 0; j < 7; j++)  // 7列
        {
            uint8_t pixel_data = english_text_7x16[ascii_index * 14 + i * 7 + j];

            if (mode == 1)  // 反色
                pixel_data = ~pixel_data;

            // 计算缓冲区位置
            uint32_t buffer_index = (start_page + i) * JLXLCD_EXT_W + x + j;

            if (buffer_index >= JLXLCD_EXT_W * JLX_PAGE_COUNT) continue;

            if (start_offset) {
                // 跨页处理
                g_frame_buffer[buffer_index] |= pixel_data >> start_offset;

                if (start_page + i + 1 < JLX_PAGE_COUNT) {
                    uint32_t next_buffer_index = (start_page + i + 1) * JLXLCD_EXT_W + x + j;
                    g_frame_buffer[next_buffer_index] |=
                        (uint8_t)(pixel_data << (8 - start_offset));
                }
            } else {
                g_frame_buffer[buffer_index] |= pixel_data;
            }
        }
    }
}

/**
 * @brief 在任意像素行显示字符串
 */
void JLX_ShowStringAnyRow(uint16_t x, uint16_t y, const char* str, uint8_t fontSize, uint8_t mode) {
    uint16_t current_x = x;
    uint16_t current_y = y;
    uint8_t chn_width = 13;  // 默认13像素宽汉字
    uint8_t eng_width = 7;   // 默认7像素宽英文

    // 根据字体大小设置宽度
    if (fontSize == 0) {
        chn_width = 13;  // 13像素宽汉字
        eng_width = 7;   // 7像素宽英文
    } else if (fontSize == 1) {
        chn_width = 16;  // 16像素宽汉字
        eng_width = 8;   // 8像素宽英文
    }

    // 只支持 mode 0（正常）和 1（反色）
    if (mode > 1) mode = 0;

    while (*str != '\0') {
        // 检查是否为汉字（中文字符最高位为1）
        if ((*str & 0x80) != 0) {
            // 汉字占用2个字节
            if (*(str + 1) == '\0') break;

            // uint16_t chn_code = (*str << 8) | *(str + 1);

            // 检查是否超出屏幕宽度
            if (current_x + chn_width > JLXLCD_EXT_W) {
                // 换行处理（如果需要）
                current_x = 0;
                current_y += 16;  // 汉字高度为16像素

                if (current_y + 16 > JLXLCD_EXT_H) break;
            }

            if (fontSize == 0) {
                show_chn_char_13x16(current_x, current_y, (char*)str, mode);
            } else if (fontSize == 1) {
                // 16x16汉字显示（需要实现相应的字库查找和显示函数）
                // 这里简化为调用13x16显示
                show_chn_char_13x16(current_x, current_y, (char*)str, mode);
            }

            current_x += chn_width;
            str += 2;
        } else {
            // 英文字符
            if (current_x + eng_width > JLXLCD_EXT_W) {
                // 换行处理
                current_x = 0;
                current_y += 16;

                if (current_y + 16 > JLXLCD_EXT_H) break;
            }

            // 特殊字符宽度调整
            uint8_t actual_eng_width = eng_width;
            if (*str == ' ')
                actual_eng_width = 5;
            else if (*str == '-')
                actual_eng_width = 6;

            if (fontSize == 0) {
                show_eng_char_7x16(current_x, current_y, *str, mode);
            } else if (fontSize == 1) {
                // 8x16英文字符显示（需要实现相应的字库查找和显示函数）
                // 这里简化为调用7x16显示
                show_eng_char_7x16(current_x, current_y, *str, mode);
            }

            current_x += actual_eng_width;
            str++;
        }
    }
}
