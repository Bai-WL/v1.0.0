/**
 * @file bsp_gd32f303xx_jlx192128g.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.4
 * @date 2025-07-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __BSP_GD32F303XX_JLX192128G_H__
#define __BSP_GD32F303XX_JLX192128G_H__

#include "stdint.h"

#define JLX_SPIDMA

#define JLXLCD_W 192
#define JLXLCD_H 128

#define JLX_SPI_RST_GPIO_PORT GPIOB
#define JLX_SPI_RST_GPIO_PIN GPIO_PIN_0

#define JLX_SPI_RS_GPIO_PORT GPIOB
#define JLX_SPI_RS_GPIO_PIN GPIO_PIN_1

#define JLX_SPI_CS_GPIO_PORT GPIOA
#define JLX_SPI_CS_GPIO_PIN GPIO_PIN_4

#define JLX_BL_GPIO_PORT GPIOC
#define JLX_BL_GPIO_PIN GPIO_PIN_6

void bsp_JLXLcdInit(void);
void bsp_JLXLcdEnterSleepMode(void);
void bsp_JLXLcdExitSleepMode(void);
void bsp_JLXLcdShowUnderline(uint32_t _usx, uint32_t _usy, uint32_t len);
void bsp_JLXLcdClearRect(uint8_t _ucx, uint8_t _ucxCnt, uint8_t _ucy, uint8_t _ucyCnt, uint8_t _ucColor);
void bsp_JLXLcdClearLine(uint8_t _ucy, uint8_t _ucyCnt, uint8_t _ucColor);
void bsp_JLXLcdClearScreen(uint8_t color);
void bsp_JLXLcdShowString(uint16_t _usx, uint16_t _usy, const char *_pcStr, uint8_t _ucSize, uint8_t _ucMode);
void bsp_JLXLcdShow(uint16_t _usx, uint16_t _usy, uint16_t _width, uint16_t _height, const char *_icon, uint8_t _ucMode);
void bsp_JLXLcdRefreshScreen(void);
void bsp_JLXLcdShowBottonIcon(const char *bottonIcon);
void bsp_JLXLcdShowString_Any_row(uint16_t _usx, uint16_t _usy, const char *_pcStr, uint8_t _ucSize, uint8_t _ucMode);

#endif /* __BSP_GD32F303XX_JLX192128G_H__ */
