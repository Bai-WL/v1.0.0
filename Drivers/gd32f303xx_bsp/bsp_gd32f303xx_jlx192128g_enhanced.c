/**
 * @file bsp_gd32f303xx_jlx192128g_enhanced.c
 * @author your name (you@domain.com)
 * @brief Enhanced LCD functions with pixel-level precision
 * @version 1.0
 * @date 2026-04-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <stdint.h>
#include <string.h>

#include "bsp_assert.h"
#include "bsp_font.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_malloc.h"
#include "gd32f30x_dma.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_spi.h"

/* External frame buffer from main LCD driver */
extern char frame_buffer_0[192 * 16];
extern char frame_buffer_1[192 * 16];
extern char* frame_buffer;

#ifdef JLX_SPIDMA
#define USE_FRAME_BUFFER
#endif

/* Function prototypes from main LCD driver */
extern unsigned long ulCHNHashMapGetIdx(unsigned long key);
extern unsigned long ulCHNHashMapGetIdx16x16(unsigned long key);

/* Local function prototypes */
static void vShowCHNChar_Any_row_Simple(unsigned short _usx, unsigned short _usy,
                                        unsigned char* _pcStr);
static void vShowENGChar_Any_row_Simple(unsigned short _usx, unsigned short _usy,
                                        unsigned char* _pcStr);

/**
 * @brief Simplified string display function with pixel-level y-coordinate
 *        Only supports _ucSize == 0 and _ucMode == 0
 *
 * @param _usx Start x coordinate (0-191)
 * @param _usy Start y coordinate in pixels (0-127)
 * @param _pcStr String to display (ASCII and Chinese)
 * @param _ucSize Font size (must be 0 for 8x16 English / 16x16 Chinese)
 * @param _ucMode Display mode (must be 0 for normal display)
 */
void bsp_JLXLcdShowString_Any_row_Simple(unsigned short _usx, unsigned short _usy,
                                         const char* _pcStr, unsigned char _ucSize,
                                         unsigned char _ucMode) {
    unsigned char _ucEntLine = 2;
    unsigned char _ucEngWSize = 8;
    unsigned char _ucChnWSize = 16;
    unsigned char _ucCharType = 0;  // english default

    /* 非法参数检测 */
    bsp_assert(_usx < JLXLCD_W);
    bsp_assert(_usy < JLXLCD_H);

    /* 根据字体大小设置调整参数 */
    _ucEntLine = 2;
#ifdef CHN13X16
    _ucChnWSize = 13; /* XXX: 修改该值，可以调整字间距 */
#else
    _ucChnWSize = 16;
#endif
    _ucEngWSize = 7;

    /* 绘制单个字符 */
    while (*_pcStr != '\0') {
        _ucCharType = (*_pcStr & 0x80);

        if ((_usx + (_ucCharType ? _ucChnWSize : _ucEngWSize)) > JLXLCD_W) {  // 换行
            _usx = 0;
            if ((_usy + _ucEntLine * 8) >= JLXLCD_H) {
                return;  // 超出屏幕底部
            } else {
                _usy += _ucEntLine * 8;  // 移动到下一行
            }
        }

        if (_ucCharType) { /* 该字符为汉字 */
            vShowCHNChar_Any_row_Simple(_usx, _usy, (unsigned char*)_pcStr);
            _pcStr += 2;
            _usx += _ucChnWSize;
        } else {
            if (*(unsigned char*)_pcStr == ' ') {
                _ucEngWSize = 5;
            } else if (*(unsigned char*)_pcStr == '-') {
                _ucEngWSize = 6;
            } else {
                _ucEngWSize = 7;
            }
            vShowENGChar_Any_row_Simple(_usx, _usy, (unsigned char*)_pcStr);
            _pcStr += 1;
            _usx += _ucEngWSize;
        }
    }
}

/**
 * @brief Display Chinese character with pixel-level y-coordinate
 *        Simplified version only for size 0 and mode 0
 *
 * @param _usx Start x coordinate
 * @param _usy Start y coordinate in pixels
 * @param _pcStr Pointer to Chinese character (2 bytes)
 */
static void vShowCHNChar_Any_row_Simple(unsigned short _usx, unsigned short _usy,
                                        unsigned char* _pcStr) {
    unsigned short usCNCnt = 0;
    unsigned long i;
    unsigned short j, k;

    /* 向上移动两个像素 */
    _usy = _usy - 2;

    /* Only support 16x16 Chinese characters */
#ifndef CHN13X16
    usCNCnt = sizeof(Chinese_text_16x16) / sizeof(FONT_CHN16_t);
#else
    usCNCnt = sizeof(Chinese_text_13x16) / sizeof(FONT_CHN13X16_t);
#endif

    /* Get character index from hash map */
#ifndef CHN13X16
    i = ulCHNHashMapGetIdx16x16(*((unsigned short*)_pcStr));
#else
    i = ulCHNHashMapGetIdx(*((unsigned short*)_pcStr));
#endif

    if (i >= usCNCnt) return;

#ifdef USE_FRAME_BUFFER
#ifdef CHN13X16
    /* 13x16 Chinese character rendering */
    unsigned short start_page = _usy / 8;   // 起始页
    unsigned char start_offset = _usy % 8;  // 起始偏移

    for (j = 0; j < 2; j++) {
        for (k = 0; k < 13; k++) {
            unsigned char pixel_data = Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k];

            if (start_offset) {
                frame_buffer[_usx + (start_page + j) * 192 + k] |= pixel_data >> start_offset;
                frame_buffer[_usx + (start_page + j + 1) * 192 + k] |=
                    (unsigned char)(pixel_data << (8 - start_offset));
            } else {
                frame_buffer[_usx + (start_page + j) * 192 + k] |= pixel_data;
            }
        }
    }
#else
    /* 16x16 Chinese character rendering */
    unsigned short start_page = _usy / 8;   // 起始页
    unsigned char start_offset = _usy % 8;  // 起始偏移

    for (j = 0; j < 2; j++) {
        for (k = 0; k < 16; k++) {
            unsigned char pixel_data = Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k];

            if (start_offset) {
                frame_buffer[_usx + (start_page + j) * 192 + k] |= pixel_data >> start_offset;
                frame_buffer[_usx + (start_page + j + 1) * 192 + k] |=
                    (unsigned char)(pixel_data << (8 - start_offset));
            } else {
                frame_buffer[_usx + (start_page + j) * 192 + k] |= pixel_data;
            }
        }
    }
#endif
#else
    /* Non-DMA mode not supported in this simplified version */
#endif
}

/**
 * @brief Display English character with pixel-level y-coordinate
 *        Simplified version only for size 0 and mode 0
 *
 * @param _usx Start x coordinate
 * @param _usy Start y coordinate in pixels
 * @param _pcStr Pointer to English character (1 byte)
 */
static void vShowENGChar_Any_row_Simple(unsigned short _usx, unsigned short _usy,
                                        unsigned char* _pcStr) {
    unsigned char usASCIndex = *_pcStr - ' ';
    unsigned short i, j;

    if (usASCIndex > 94) {  // ASCII位置超限
        return;
    }

#ifdef USE_FRAME_BUFFER
    unsigned short start_page = _usy / 8;   // 起始页
    unsigned char start_offset = _usy % 8;  // 起始偏移

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 7; j++) {
            unsigned char pixel_data = english_text_7x16[(usASCIndex * 14) + (i * 7) + j];

            if (start_offset) {
                frame_buffer[_usx + (start_page + i) * 192 + j] |= pixel_data >> start_offset;
                frame_buffer[_usx + (start_page + i + 1) * 192 + j] |=
                    (unsigned char)(pixel_data << (8 - start_offset));
            } else {
                frame_buffer[_usx + (start_page + i) * 192 + j] |= pixel_data;
            }
        }
    }
#else
    /* Non-DMA mode not supported in this simplified version */
#endif
}

/**
 * @brief Clear a rectangular area with pixel-level height
 *
 * @param _ucx Start x coordinate (0-191)
 * @param _ucxCnt Width in pixels (0-191)
 * @param _ucy Start y coordinate in pixels (0-127)
 * @param _ucyCnt Height in pixels (0-127)
 * @param _ucColor 0x00 (white) or 0xff (black)
 */
void bsp_JLXLcdClearRect_Pixel(uint8_t _ucx, uint8_t _ucxCnt, uint16_t _ucy, uint16_t _ucyCnt,
                               uint8_t _ucColor) {
    int i, j;

    /* Parameter validation */
    if (_ucx >= JLXLCD_W || _ucy >= JLXLCD_H) return;

    if (_ucx + _ucxCnt > JLXLCD_W) _ucxCnt = JLXLCD_W - _ucx;

    if (_ucy + _ucyCnt > JLXLCD_H) _ucyCnt = JLXLCD_H - _ucy;

#ifdef USE_FRAME_BUFFER
    /* Calculate start and end rows in terms of 8-pixel lines */
    uint16_t start_line = _ucy / 8;
    uint16_t end_line = (_ucy + _ucyCnt - 1) / 8;
    uint8_t start_bit_offset = _ucy % 8;
    uint8_t end_bit_offset = (_ucy + _ucyCnt - 1) % 8;

    /* Create mask for clearing/setting bits */
    uint8_t color_mask = _ucColor ? 0xFF : 0x00;

    /* For each column in the rectangle */
    for (j = _ucx; j < _ucx + _ucxCnt && j < JLXLCD_W; j++) {
        /* For each 8-pixel line affected by this rectangle */
        for (i = start_line; i <= end_line && i < 16; i++) {
            uint8_t line_mask = 0xFF;

            /* First line: mask out bits above start offset */
            if (i == start_line && start_bit_offset != 0) {
                line_mask &= (0xFF << start_bit_offset);
            }

            /* Last line: mask out bits below end offset */
            if (i == end_line && end_bit_offset != 7) {
                line_mask &= (0xFF >> (7 - end_bit_offset));
            }

            /* Apply the mask to set/clear bits */
            if (_ucColor == 0xFF) {
                /* Set bits to 1 (black) */
                frame_buffer[i * 192 + j] |= line_mask;
            } else {
                /* Clear bits to 0 (white) */
                frame_buffer[i * 192 + j] &= ~line_mask;
            }
        }
    }
#else
    /* Non-DMA mode: use existing vLcdSetCursor with pixel-to-line conversion */
    uint8_t start_line = _ucy / 8;
    uint8_t line_count = ((_ucy + _ucyCnt - 1) / 8) - start_line + 1;

    vLcdSetCursor(_ucx, start_line, _ucxCnt, line_count);

    /* For non-DMA mode, we need to handle partial lines properly */
    /* This is simplified - actual implementation would need to handle bit-level operations */
    for (i = 0; i < line_count; i++) {
        for (j = 0; j < _ucxCnt; j++) {
            vSpiSendData(_ucColor);
        }
    }
#endif
}