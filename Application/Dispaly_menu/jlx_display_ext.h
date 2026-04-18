/**
 * @file jlx_display_ext.h
 * @brief 扩展JL192128 LCD显示函数库 - 支持像素级高度和可配置字体
 * @version 1.0
 * @date 2026-04-16
 *
 * @copyright Copyright (c) 2026
 *
 * 说明：
 * 1. 本文件提供两个增强的LCD显示函数，可独立使用
 * 2. 需要包含texture.h以获取字库数据
 * 3. 使用前必须调用JLX_DisplayExt_Init设置帧缓冲区地址
 */

#ifndef __JLX_DISPLAY_EXT_H__
#define __JLX_DISPLAY_EXT_H__

#include <stdint.h>

// LCD屏幕尺寸定义
#define JLXLCD_EXT_W 192
#define JLXLCD_EXT_H 128

/**
 * @brief 初始化显示扩展库
 * @param frame_buffer 帧缓冲区地址（大小为192*16字节）
 */
void JLX_DisplayExt_Init(char** frame_buffer);

/**
 * @brief 清除矩形区域（像素级高度）
 * @param x 起始列坐标（0-191）
 * @param y 起始行坐标（像素，0-127）
 * @param width 矩形宽度（像素）
 * @param height 矩形高度（像素）
 * @param color 颜色 0x00（白色）或 0xFF（黑色）
 */
void JLX_ClearRectPixel(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);

/**
 * @brief 在任意像素行显示字符串
 * @param x 起始列坐标（0-191）
 * @param y 起始行坐标（像素，0-127）
 * @param str 要显示的字符串（支持中文和英文）
 * @param fontSize 字体大小：0-13像素宽中文字体，1-16像素宽中文字体
 * @param mode 显示模式：0-正常显示，1-反色显示
 */
void JLX_ShowStringAnyRow(uint16_t x, uint16_t y, const char* str, uint8_t fontSize, uint8_t mode);

#endif /* __JLX_DISPLAY_EXT_H__ */
