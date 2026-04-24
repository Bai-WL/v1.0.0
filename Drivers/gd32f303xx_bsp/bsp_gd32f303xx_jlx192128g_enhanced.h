/**
 * @file bsp_gd32f303xx_jlx192128g_enhanced.h
 * @author your name (you@domain.com)
 * @brief Enhanced LCD functions with pixel-level precision
 * @version 1.0
 * @date 2026-04-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef __BSP_GD32F303XX_JLX192128G_ENHANCED_H__
#define __BSP_GD32F303XX_JLX192128G_ENHANCED_H__

#include "bsp_gd32f303xx_jlx192128g.h"

#ifdef __cplusplus
extern "C" {
#endif

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
void bsp_JLXLcdShowString_Any_row_Simple(uint16_t _usx, uint16_t _usy, const char* _pcStr, uint8_t _ucSize, uint8_t _ucMode);

/**
 * @brief Clear a rectangular area with pixel-level height
 *
 * @param _ucx Start x coordinate (0-191)
 * @param _ucxCnt Width in pixels (0-191)
 * @param _ucy Start y coordinate in pixels (0-127)
 * @param _ucyCnt Height in pixels (0-127)
 * @param _ucColor 0x00 (white) or 0xff (black)
 */
void bsp_JLXLcdClearRect_Pixel(uint8_t _ucx, uint8_t _ucxCnt, uint16_t _ucy, uint16_t _ucyCnt, uint8_t _ucColor);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_GD32F303XX_JLX192128G_ENHANCED_H__ */