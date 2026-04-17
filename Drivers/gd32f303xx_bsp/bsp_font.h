/**
 * @file bsp_font.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-01-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef __BSP_FONT_H__
#define __BSP_FONT_H__

#include "stdint.h"

#define CHN13X16 /* 使用13X16中文字库，行间距4，字间距2 */

// clang-format off
typedef struct
{
    uint8_t _ucIndex[2];
    uint8_t _ucCHN_Code[32];
} FONT_CHN16X16_t;

typedef struct
{
    uint8_t _ucIndex[2];
    uint8_t _ucCHN_Code[26];
} FONT_CHN13X16_t;

// 声明字体数据
extern const FONT_CHN16X16_t Chinese_text_16x16[];
extern const char english_text_8x16[];
extern const FONT_CHN13X16_t Chinese_text_13x16[];
extern const char english_text_7x16[];

// clang-format on
int usCNCntGet_13x16 (void);
int usCNCntGet_16x16 (void); 
#endif  //__BSP_FONT_H__

