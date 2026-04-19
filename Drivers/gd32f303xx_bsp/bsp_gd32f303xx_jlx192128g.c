/**
 * @file bsp_gd32f303xx_jlx192128g.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.8
 * @date 2025-04-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <string.h>
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_dma.h"
#include "gd32f30x_spi.h"
#include "bsp_assert.h"
#include "bsp_malloc.h"
#include "bsp_gd32f303xx_jlx192128g.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_font.h"
#include "jlx_display_ext.h"

#define LCD_SPI_INTERFACE SPI0

#define JLX_SPI_RST_GPIO_CLK_ENABLE()       \
    do                                      \
    {                                       \
        rcu_periph_clock_enable(RCU_GPIOB); \
    } while (0)

#define JLX_SPI_RS_GPIO_CLK_ENABLE()        \
    do                                      \
    {                                       \
        rcu_periph_clock_enable(RCU_GPIOB); \
    } while (0)

#define JLX_SPI_CS_GPIO_CLK_ENABLE()        \
    do                                      \
    {                                       \
        rcu_periph_clock_enable(RCU_GPIOA); \
    } while (0)

#define JLX_BL_GPIO_CLK_ENABLE()            \
    do                                      \
    {                                       \
        rcu_periph_clock_enable(RCU_GPIOC); \
    } while (0)

// @todo 需要编写通配符数据显示函数
#define SPI0_DATA_ADDRESS 0x4001300C

#define JLX_SPI_RST(x)                                                                                                               \
    do                                                                                                                               \
    {                                                                                                                                \
        x ? gpio_bit_set(JLX_SPI_RST_GPIO_PORT, JLX_SPI_RST_GPIO_PIN) : gpio_bit_reset(JLX_SPI_RST_GPIO_PORT, JLX_SPI_RST_GPIO_PIN); \
    } while (0)
#define JLX_SPI_RS(x)                                                                                                            \
    do                                                                                                                           \
    {                                                                                                                            \
        x ? gpio_bit_set(JLX_SPI_RS_GPIO_PORT, JLX_SPI_RS_GPIO_PIN) : gpio_bit_reset(JLX_SPI_RS_GPIO_PORT, JLX_SPI_RS_GPIO_PIN); \
    } while (0)
#define JLX_SPI_CS(x)                                                                                                            \
    do                                                                                                                           \
    {                                                                                                                            \
        x ? gpio_bit_set(JLX_SPI_CS_GPIO_PORT, JLX_SPI_CS_GPIO_PIN) : gpio_bit_reset(JLX_SPI_CS_GPIO_PORT, JLX_SPI_CS_GPIO_PIN); \
    } while (0)
#define JLX_BL(x)                                                                                                \
    do                                                                                                           \
    {                                                                                                            \
        x ? gpio_bit_set(JLX_BL_GPIO_PORT, JLX_BL_GPIO_PIN) : gpio_bit_reset(JLX_BL_GPIO_PORT, JLX_BL_GPIO_PIN); \
    } while (0)

static void vCHNHashMake(void);
static void vCHNHashMake16x16(void);
static void vLcdSoftInit(void);
static void vLcdDmaInit(void);
static void vSpiSendData(uint8_t byte);
static void vSpiSendCmd(uint8_t byte);
static void vLcdSetCursor(uint8_t sx, uint8_t sy, uint8_t lenx, uint8_t leny);
static void vShowCHNChar(uint16_t _usx, uint16_t _usy, uint8_t *_pcStr, uint8_t _ucSize, uint8_t _ucMode);
static void vShowENGChar(uint16_t _usx, uint16_t _usy, uint8_t *_pcStr, uint8_t _ucSize, uint8_t _ucMode);

// 外部声明字体数据
extern const FONT_CHN16X16_t Chinese_text_16x16[];
extern const char english_text_8x16[];
extern const FONT_CHN13X16_t Chinese_text_13x16[];
extern const char english_text_7x16[];

static char frame_buffer_0[192 * 16] = {0};
static char frame_buffer_1[192 * 16] = {0};
static char *frame_buffer = frame_buffer_0;

#ifdef JLX_SPIDMA
/**
 * @brief SPI0 TX(MOSI) according to DMA0_CH2
 *
 */
static void vLcdDmaInit(void)
{
    dma_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA0);

    /* */
    dma_struct_para_init(&dma_init_struct);
    dma_deinit(DMA0, DMA_CH2);

    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = (uint32_t)frame_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = 1;
    dma_init_struct.periph_addr = SPI0_DATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_LOW; /* FIXME: 需要对优先级进行核对 */
    dma_init(DMA0, DMA_CH2, &dma_init_struct);

    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH2);
    dma_memory_to_memory_disable(DMA0, DMA_CH2);
    dma_channel_enable(DMA0, DMA_CH2);
    spi_dma_enable(SPI0, SPI_DMA_TRANSMIT); /* 预先置位FTF */
}
#endif

/**
 * @brief LCD初始化函数
 *
 */
void bsp_JLXLcdInit(void)
{
    spi_parameter_struct spi_init_struct = {0};

    JLX_SPI_RST_GPIO_CLK_ENABLE();
    JLX_SPI_RS_GPIO_CLK_ENABLE();
    JLX_SPI_CS_GPIO_CLK_ENABLE();
    JLX_BL_GPIO_CLK_ENABLE();

    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SPI0);

    /* SPI0_SCK(PA5), SPI0_MISO(PA6) and SPI0_MOSI(PA7) GPIO pin configuration */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5 | GPIO_PIN_7);
    // gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    /* SPI0_CS(PA4) GPIO pin configuration */
    gpio_init(JLX_SPI_CS_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, JLX_SPI_CS_GPIO_PIN);
    gpio_init(JLX_SPI_RS_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, JLX_SPI_RS_GPIO_PIN);
    gpio_init(JLX_SPI_RST_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, JLX_SPI_RST_GPIO_PIN);
    gpio_init(JLX_BL_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, JLX_BL_GPIO_PIN);

    /* chip select invalid*/
    JLX_SPI_CS(1);

    /* deinit spi */
    spi_i2s_deinit(LCD_SPI_INTERFACE);

    /* LCD_SPI_INTERFACE parameter config */
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_8; // 60/8 = 7.5M
    spi_init_struct.endian = SPI_ENDIAN_MSB;
    spi_init(LCD_SPI_INTERFACE, &spi_init_struct);

    /* enable LCD_SPI_INTERFACE */
    spi_enable(LCD_SPI_INTERFACE);

    vLcdSoftInit();

    JLX_BL(1); /* open back light */

#ifdef JLX_SPIDMA
    vLcdDmaInit();
    vLcdSetCursor(0, 0, 192, 16); /* 进行一次窗口配置 */
    bsp_JLXLcdClearScreen(0x00);
    bsp_JLXLcdRefreshScreen();
    JLX_DisplayExt_Init(&frame_buffer);
#endif
}

/**
 * @brief Stop dma channel and command lcd module to enter sleep, close back light.
 *
 */
void bsp_JLXLcdEnterSleepMode(void)
{
    volatile uint32_t dly = 20000;
#ifdef JLX_SPIDMA
    dma_channel_disable(DMA0, DMA_CH2);
    while (dly--)
        ;
#endif
    vSpiSendCmd(0x30); // EXT=0
    vSpiSendCmd(0x95); // Enter Sleep
    JLX_BL(0);
    dly = 20000;
    while (dly--)
        ;
}

/**
 * @brief Wake up lcd module and open back light, restart dma transmit.
 *
 */
void bsp_JLXLcdExitSleepMode(void)
{
    vSpiSendCmd(0x30); // EXT=0
    vSpiSendCmd(0x94); // Sleep out
#ifdef JLX_SPIDMA
    vLcdDmaInit();
    vLcdSetCursor(0, 0, 192, 16); /* 进行一次窗口配置 */
#endif
    JLX_BL(1);
}

void bsp_JLXLcdShowUnderline(uint32_t _usx, uint32_t _usy, uint32_t len)
{
    /* 非法参数检测 */
    bsp_assert(_usx < JLXLCD_W);
    bsp_assert(_usy < (JLXLCD_H / 8));

    for (; len != 0; len--)
    {
        frame_buffer[_usy * JLXLCD_W + _usx] |= 0x01;
        if (++_usx == JLXLCD_W)
            return;
    }
}
/**
 * @brief 字符显示，可显示字母或汉字
 *
 * @param _usx
 * @param _usy  // 防止参数检测时溢出误判
 * @param _pString
 * @param size  0为正常大小，1为大字符(大字符目前不支持使用)
 * @param mode
 */
void bsp_JLXLcdShowString(uint16_t _usx, uint16_t _usy, const char *_pcStr, uint8_t _ucSize, uint8_t _ucMode)
{
    uint8_t _ucEntLine = 2;
    uint8_t _ucEngWSize = 8;
    uint8_t _ucChnWSize = 16;
    uint8_t _ucCharType = 0; // english default

    /* 非法参数检测 */
    bsp_assert(_usx < JLXLCD_W);
    bsp_assert(_usy < (JLXLCD_H / 8));

    /* 根据字体大小设置调整参数 */
    if (_ucSize == 0)
    {
        _ucEntLine = 2;
#ifdef CHN13X16
        _ucChnWSize = 13; /* XXX: 修改该值，可以调整字间距 */
#else
        _ucChnWSize = 16;
#endif
        _ucEngWSize = 8;
    }
    else if (_ucSize == 1)
    {
        _ucEntLine = 2;
        _ucChnWSize = 18;
        _ucEngWSize = 8;
    }
    else
    {
        _ucEntLine = 3;
        _ucChnWSize = 24;
        _ucEngWSize = 12;
    }

    /* 绘制单个字符 */
    while (*_pcStr != '\0')
    {
        _ucCharType = (*_pcStr & 0x80);

        if ((_usx + (_ucCharType ? _ucChnWSize : _ucEngWSize)) > JLXLCD_W)
        { // 换行
            _usx = 0;
            if ((_usy + _ucEntLine) * 8 >= JLXLCD_H)
            {
                return;
            }
            else
            {
                return;
            }
        }

        if (_ucCharType)
        { /* 该字符为汉字 */
            vShowCHNChar(_usx, _usy, (uint8_t *)_pcStr, _ucSize, _ucMode);
            _pcStr += 2;
            _usx += _ucChnWSize;
        }
        else
        {
            vShowENGChar(_usx, _usy, (uint8_t *)_pcStr, _ucSize, _ucMode);
            _pcStr += 1;
            _usx += _ucEngWSize;
        }
    }
}

/**
 * @brief LCD 浮点型数据显示，仅对小数点进行特殊处理
 *       （注：该函数不支持换行，需注意字符数组长度）
 *
 * @param _usx 起始列坐标
 * @param _usy 起始行坐标
 * @param _pcStr 需要提前使用sprintf()函数将浮点数据转换为为指定对齐、指定小数点数、指定补零的字符数组
 * @param _ucSize 字符大小，目前仅支持8x16=0
 * @param _ucMode =0为正常显示，=1为反显
 */
void bsp_JLXLcdShowFloatNum(uint16_t _usx, uint16_t _usy, uint8_t *_pcStr, uint8_t _ucSize, uint8_t _ucMode)
{
    uint8_t usASCIndex = *_pcStr - ' ';
    uint8_t i;
#ifdef JLX_SPIDMA
    if (usASCIndex > 94) // ASCII位置超限
        return;

    while (*_pcStr != '\0')
    {
        if (_ucSize == 0)
        { // 8x16字符
            if (*_pcStr == '.')
            {
                frame_buffer[(_usy + 1) * 192 + _usx - 1] &= 0x80;
            }
            else if (*_pcStr > '9' || *_pcStr < '0')
            {
                return;
            }
            else
            {
                for (i = 0; i < 16; i++)
                {
                    if (!_ucMode)
                    {
                        frame_buffer[_usx + (_usy + i / 8) * 192 + (i % 8)] = english_text_8x16[usASCIndex * 16 + i];
                    }
                    else
                    {
                        frame_buffer[_usx + (_usy + i / 8) * 192 + (i % 8)] = ~(english_text_8x16[usASCIndex * 16 + i]);
                    }
                }
                _usx += 8;
            }
        }
        else
        {
            __NOP();
        }
        _pcStr += 1;
    }

#else
#endif
}

/**
 * @brief 清除一片长方形区域
 *
 * @param _ucx 起始列坐标
 * @param _ucxCnt 列宽
 * @param _ucy 起始行坐标   （注：液晶屏以8像素为一行, 实际行总数仅16行）
 * @param _ucyCnt 行宽
 * @param _ucColor 0x00（全白） 或 0xff（全黑）
 */
void bsp_JLXLcdClearRect(uint8_t _ucx, uint8_t _ucxCnt, uint8_t _ucy, uint8_t _ucyCnt, uint8_t _ucColor)
{
    int i, j;
#ifdef JLX_SPIDMA
    for (i = 0; i < _ucyCnt && (_ucy + i) < JLXLCD_H / 8; i++)
    { /* 数组边界保护 */
        for (j = _ucx; j < _ucxCnt + _ucx && j < JLXLCD_W; j++)
        { /* 数组边界保护 */
            frame_buffer[(_ucy + i) * 192 + j] = _ucColor;
        }
    }
#else
    vLcdSetCursor(_ucx, _ucy, _ucxCnt, _ucyCnt);
    for (i = 0; i < _ucyCnt; i++)
    {
        for (j = 0; j < _ucxCnt; j++)
        {
            vSpiSendData(_ucColor);
        }
    }
#endif
}

/**
 * @brief 以8像素行形式填充整行
 *
 * @param _ucy 起始行坐标
 * @param _ucyCnt 行数
 * @param _ucColor 填充阵列
 */
void bsp_JLXLcdClearLine(uint8_t _ucy, uint8_t _ucyCnt, uint8_t _ucColor)
{
    int i, j;
#ifdef JLX_SPIDMA
    for (i = 0; i < _ucyCnt && (_ucy + i) < JLXLCD_H / 8; i++)
    { /* 数组边界保护 */ /* FIXME: 紧急bug修复, 此处数组越界 */
        for (j = 0; j < 192; j++)
        {
            frame_buffer[(_ucy + i) * 192 + j] = _ucColor;
        }
    }
#else
    vLcdSetCursor(0, _ucy, 192, _ucyCnt);
    for (i = 0; i < _ucyCnt; i++)
    {
        for (j = 0; j < 192; j++)
        {
            vSpiSendData(_ucColor);
        }
    }
#endif
}

void bsp_JLXLcdClearScreen(uint8_t color)
{
#ifdef JLX_SPIDMA
    memset(frame_buffer, color, 192 * 16);
#else
    uint8_t i = 0, j = 0;
    /* set cursor */
    vLcdSetCursor(0, 0, 192, 16);
    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < 192; j++)
        {
            vSpiSendData(color);
        }
    }
#endif
}

/**
 * @brief DMA屏幕刷新
 *
 */
void bsp_JLXLcdRefreshScreen(void)
{
#ifdef JLX_SPIDMA
    while (!dma_flag_get(DMA0, DMA_CH2, DMA_FLAG_FTF))
        ;
    dma_flag_clear(DMA0, DMA_CH2, DMA_FLAG_FTF);
    while (spi_i2s_flag_get(SPI0, SPI_FLAG_TRANS))
        ;
    JLX_SPI_CS(1);

    dma_channel_disable(DMA0, DMA_CH2);
    dma_memory_address_config(DMA0, DMA_CH2, (uint32_t)frame_buffer);
    if (frame_buffer == frame_buffer_0)
    {
        frame_buffer = frame_buffer_1;
    }
    else
    {
        frame_buffer = frame_buffer_0;
    }
    dma_transfer_number_config(DMA0, DMA_CH2, 192 * 16);
    JLX_SPI_RS(1);
    JLX_SPI_CS(0); // 片选，必须在传输开始之前片选，否则影响屏幕显示
    dma_channel_enable(DMA0, DMA_CH2);
    spi_dma_enable(SPI0, SPI_DMA_TRANSMIT);
#endif
}

/**
 * @brief LCD软件初始化
 *
 */
static void vLcdSoftInit(void)
{
    vCHNHashMake();
    vCHNHashMake16x16();
    JLX_SPI_RST(0);
    bsp_delay(100);
    JLX_SPI_RST(1);
    bsp_delay(100);

    // vSpiSendCmd(0x30);  // EXT = 0
    // vSpiSendCmd(0xaf);  // display on
    // vSpiSendCmd(0xa6);  // 正常显示
    // vSpiSendCmd(0x23);  // 打开所有像素

    // vSpiSendCmd(0xca);
    // vSpiSendData(0x00);
    // vSpiSendData(0x4f);
    // vSpiSendData(0x0e);

    // vSpiSendCmd(0x94);  //推出sleep模式

    // vSpiSendCmd(0x75);
    // vSpiSendData(0x00);
    // vSpiSendData(0x0f); //128

    // vSpiSendCmd(0x15);
    // vSpiSendData(0x00);
    // vSpiSendData(0xbf); // 191

    // vSpiSendCmd(0xbc);
    // vSpiSendData(0x00);

    // vSpiSendCmd(0xd1);

    // vSpiSendCmd(0xa3);
    // vSpiSendData(0x0b);

    // vSpiSendCmd(0x81);
    // vSpiSendData(0x1f);
    // vSpiSendData(0x03);

    // vSpiSendCmd(0xd6);

    // vSpiSendCmd(0x08);

    // vSpiSendCmd(0xf0);
    // vSpiSendData(0x10);

    // vSpiSendCmd(0x6e);

    // vSpiSendCmd(0x76);

    // vSpiSendCmd(0x32);
    // vSpiSendData(0x00);
    // vSpiSendData(0x01);
    // vSpiSendData(0x04);

    // vSpiSendCmd(0x51);
    // vSpiSendData(0xfb);

    // vSpiSendCmd(0x40);

    // vSpiSendCmd(0x5c);

    vSpiSendCmd(0x30);  // EXT=0
    vSpiSendCmd(0x94);  // Sleep out
                        // vSpiSendCmd(0xa7);
    vSpiSendCmd(0x31);  // EXT=1
    vSpiSendCmd(0xD7);  // Autoread disable
    vSpiSendData(0X9F); //
    vSpiSendCmd(0x32);  // Analog SET
    vSpiSendData(0x00); // OSC Frequency adjustment
    vSpiSendData(0x01); // Frequency on booster capacitors->6KHz
    vSpiSendData(0x02); // Bias=1/12
    vSpiSendCmd(0x31);  // Analog SET
    vSpiSendCmd(0xf2);  // 温度补偿
    vSpiSendData(0x1e); // OSC Frequency adjustment
    vSpiSendData(0x28); // Frequency on booster capacitors->6KHz
    vSpiSendData(0x32); //
    vSpiSendCmd(0x20);  // Gray Level
    vSpiSendData(0x01);
    vSpiSendData(0x03);
    vSpiSendData(0x05);
    vSpiSendData(0x07);
    vSpiSendData(0x09);
    vSpiSendData(0x0b);
    vSpiSendData(0x0d);
    vSpiSendData(0x10);
    vSpiSendData(0x11);
    vSpiSendData(0x13);
    vSpiSendData(0x15);
    vSpiSendData(0x17);
    vSpiSendData(0x19);
    vSpiSendData(0x1b);
    vSpiSendData(0x1d);
    vSpiSendData(0x1f);
    vSpiSendCmd(0x30);  // EXT=0
    vSpiSendCmd(0x75);  // Page Address setting
    vSpiSendData(0X00); // XS=0
    vSpiSendData(0X14); // XE=159
    vSpiSendCmd(0x15);  // Clumn Address setting
    vSpiSendData(0X00); // XS=0
    vSpiSendData(0Xbf); // XE=256
    vSpiSendCmd(0xBC);  // Data scan direction
    vSpiSendData(0x02); // MX.MY=Normal
    vSpiSendCmd(0x08);  // 数据格式，0x0C：表示选择LSB（DB0)在顶; 0x08:表示选择LSB(DB0)在底
    vSpiSendCmd(0xCA);  // Display Control
    vSpiSendData(0X00); //
    vSpiSendData(0X7F); // Duty=128
    vSpiSendData(0X20); // Nline=off
    vSpiSendCmd(0xf0);  // Display Mode
    vSpiSendData(0X10); // 10=Monochrome Mode,11=4Gray
    vSpiSendCmd(0x81);  // EV control /*调对比度,VOP=15.05V*/
    vSpiSendData(0x1e); // VPR[5-0] /*微调对比度的值，可设置范围0x00～0x3f*/
    vSpiSendData(0x04); // VPR[8-6] /*粗调对比度，可设置范围0x00～0x07*/
    vSpiSendCmd(0x20);  // Power control
    vSpiSendData(0x0B); // D0=regulator ; D1=follower ; D3=booste, on:1 off:0
    bsp_delay(1);
    vSpiSendCmd(0xAF); // Display on
}

static inline uint32_t ulCHNHashFunc(const uint32_t key)
{
    return (key + (key >> 4)) & 0x1FF;
}

struct chn_hash_list
{
    struct chn_hash_list *next;
    uint32_t idx;
};

uint8_t hash_init_flag = 0;
struct chn_hash_list *chn_hash_13x16[512];
static void vCHNHashMake(void)
{
    int i;
    uint16_t b16tmp;
    uint32_t b32tmp;
    int usCNCnt = usCNCntGet_13x16();
    struct chn_hash_list *pNewNode;
    struct chn_hash_list *pCurr;

    /* 避免重复初始化 */
    if (hash_init_flag != 0)
        return;

    /* 清空所有桶 */
    memset(chn_hash_13x16, 0, 512 * sizeof(struct chn_hash_list *));
    hash_init_flag = 1;

    for (i = 0; i < usCNCnt; i++)
    {
        /* 获取汉字编码（小端序，取决于实际存储） */
        b16tmp = *((uint16_t *)Chinese_text_13x16[i]._ucIndex);
        b32tmp = ulCHNHashFunc(b16tmp);  /* 计算哈希桶索引 0~511 */
        if(b32tmp == 0x153)
        {
            __NOP();
        }
        /* 分配新节点 */
        pNewNode = (struct chn_hash_list *)bsp_malloc(sizeof(struct chn_hash_list));
        if (pNewNode == NULL)
        {
            bsp_assert(1);  /* 内存分配失败，系统死机 */
        }
        pNewNode->idx = i;
        pNewNode->next = NULL;

        /* 插入到对应桶的链表尾部 */
        if (chn_hash_13x16[b32tmp] == NULL)
        {
            /* 桶为空，新节点成为头节点 */
            chn_hash_13x16[b32tmp] = pNewNode;
        }
        else
        {
            /* 遍历到链表末尾 */
            pCurr = chn_hash_13x16[b32tmp];
            while (pCurr->next != NULL)
            {
                pCurr = pCurr->next;
            }
            pCurr->next = pNewNode;
        }
    }
}
uint8_t hash_init_flag_16x16 = 0;
struct chn_hash_list *chn_hash_16x16[512];
static void vCHNHashMake16x16(void)
{
    int i;
    uint16_t b16tmp;
    int b32tmp;
    int usCNCnt = usCNCntGet_16x16();
    struct chn_hash_list *ptmp;

    if (hash_init_flag_16x16 != 0)
        return;

    memset(chn_hash_16x16, 0, 512 * sizeof(struct chn_hash_list *));
    hash_init_flag_16x16 = 1;
    for (i = 0; i < usCNCnt; i++)
    {
        b16tmp = *((uint16_t *)Chinese_text_16x16[i]._ucIndex);
        b32tmp = ulCHNHashFunc(b16tmp);
        ptmp = (struct chn_hash_list *)(chn_hash_16x16 + b32tmp);
        while (ptmp->next != NULL)
        {
            ptmp = ptmp->next;
        }
        ptmp->next = (struct chn_hash_list *)bsp_malloc(sizeof(struct chn_hash_list));
        if (ptmp->next != NULL)
        {
            memset(ptmp->next, 0, sizeof(struct chn_hash_list));
            ptmp->next->idx = i;
        }
        else
        {
            bsp_assert(1);
        }
    }
}

uint32_t ulCHNHashMapGetIdx(const uint32_t key)
{
    struct chn_hash_list *pNode;
    uint32_t b32tmp;
    uint16_t b16tmp;
    uint32_t ret = 0xffff;

    /* 哈希表未初始化，返回无效索引 */
    if (!hash_init_flag)
        return 0xffff;

    /* 计算哈希桶索引 */
    b32tmp = ulCHNHashFunc(key);

    /* 获取对应桶的链表头 */
    pNode = chn_hash_13x16[b32tmp];

    /* 遍历链表，查找编码匹配的节点 */
    while (pNode != NULL)
    {
        /* 取出节点中存储的字库索引对应的汉字编码 */
        b16tmp = *((uint16_t *)Chinese_text_13x16[pNode->idx]._ucIndex);
        if (b16tmp == key)
        {
            ret = pNode->idx;   /* 找到，返回字库索引 */
            break;
        }
        pNode = pNode->next;    /* 继续下一个节点 */
    }

    return ret;
}

static uint32_t ulCHNHashMapGetIdx16x16(const uint32_t key)
{
    struct chn_hash_list *ptmp;
    uint16_t b16tmp;
    uint32_t b32tmp;
    uint32_t ret = 0xffff;
    if (!hash_init_flag_16x16)
        return 0xffff;

    b32tmp = ulCHNHashFunc(key);
    ptmp = (struct chn_hash_list *)(chn_hash_16x16 + b32tmp);
    while (ptmp->next != NULL)
    {
        b16tmp = *((uint16_t *)Chinese_text_16x16[ptmp->next->idx]._ucIndex); /* FIXME：此处读数据需要做边界保护 */
        if (b16tmp == key)
        {
            ret = ptmp->next->idx;
            break;
        }
        ptmp = ptmp->next;
    }

    return ret;
}

/**
 * @brief 绘制中文
 *
 * @param _usx
 * @param _usy
 * @param _pcStr
 * @param _ucSize
 * @param _ucMode
 */
static void vShowCHNChar(uint16_t _usx, uint16_t _usy, uint8_t *_pcStr, uint8_t _ucSize, uint8_t _ucMode)
{
    uint16_t usCNCnt = 0;
    uint32_t i, j, k;
    if (_ucSize == 0)
    { // 16x16字符
#ifdef CHN13X16
        usCNCnt = usCNCntGet_13x16();
#else
        usCNCnt = usCNCntGet_16x16();
#endif
        i = ulCHNHashMapGetIdx(*((uint16_t *)_pcStr));
        if (i >= usCNCnt)
            return;
#ifdef JLX_SPIDMA
#ifdef CHN13X16
        for (j = 0; j < 2; j++)
        {
            for (k = 0; k < 13; k++)
            {
                switch (_ucMode)
                {
                case 0:
                    frame_buffer[_usx + (_usy + j) * 192 + k] ^= Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k];
                    break;
                case 1:
                    frame_buffer[_usx + (_usy + j) * 192 + k] ^= ~(Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k]);
                    break;
                case 2:
                    if (j == 1)
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] = (Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k]) | 0x01;
                    }
                    else
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] = (Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k]);
                    }
                case 3:
                    if (((_usx + (_usy + j) * 192 + k) % 192 >= 1) && ((_usx + (_usy + j) * 192 + k) % 192 <= 192))
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] ^= Chinese_text_13x16[i]._ucCHN_Code[j * 13 + k];
                    }
                    break;
                }
            }
        }
#else
        for (j = 0; j < 32; j++)
        {
            if (!_ucMode)
            {
                frame_buffer[_usx + (_usy + j / 16) * 192 + (j % 16)] = Chinese_text_16x16[i]._ucCHN_Code[j];
            }
            else
            {
                frame_buffer[_usx + (_usy + j / 16) * 192 + (j % 16)] = ~(Chinese_text_16x16[i]._ucCHN_Code[j]);
            }
        }
#endif
#else
#ifdef CHN13X16
        vLcdSetCursor(_usx, _usy, 13, 2);
        for (j = 0; j < 26; j++)
        {
            if (!_ucMode)
            {
                vSpiSendData(Chinese_text_13x16[i]._ucCHN_Code[j]);
            }
            else
            {
                vSpiSendData(~(Chinese_text_13x16[i]._ucCHN_Code[j]));
            }
        }
#else
        vLcdSetCursor(_usx, _usy, 16, 2);
        for (j = 0; j < 32; j++)
        {
            if (!_ucMode)
            {
                vSpiSendData(Chinese_text_16x16[i]._ucCHN_Code[j]);
            }
            else
            {
                vSpiSendData(~(Chinese_text_16x16[i]._ucCHN_Code[j]));
            }
        }
#endif
#endif
    }
    else if (_ucSize == 1)
    { // 16x16字符
        usCNCnt = usCNCntGet_16x16();
        i = ulCHNHashMapGetIdx16x16(*((uint16_t *)_pcStr));
        if (i >= usCNCnt)
            return;
#ifdef JLX_SPIDMA
        //        for (j = 0; j < 32; j++)
        //        {
        //            if (!_ucMode)
        //            {
        //                frame_buffer[_usx + (_usy + j / 16) * 192 + (j % 16)] = Chinese_text_16x16[i]._ucCHN_Code[j];
        //            }
        //            else if(_ucMode == 1)
        //            {
        //                frame_buffer[_usx + (_usy + j / 16) * 192 + (j % 16)] = ~(Chinese_text_16x16[i]._ucCHN_Code[j]);
        //            }
        //						else if(_ucMode == 3)
        //            {
        //								if(((_usx + (_usy + j / 16) * 192 + (j % 16))%192 >=20)&&((_usx + (_usy + j / 16) * 192 + (j % 16))%192<=172))
        //                frame_buffer[_usx + (_usy + j / 16) * 192 + (j % 16)] ^= (Chinese_text_16x16[i]._ucCHN_Code[j]);
        //            }
        //        }
        for (j = 0; j < 2; j++)
        {
            for (k = 0; k < 16; k++)
            {
                switch (_ucMode)
                {
                case 0:
                    frame_buffer[_usx + (_usy + j) * 192 + k] ^= Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k];
                    break;
                case 1:
                    frame_buffer[_usx + (_usy + j) * 192 + k] ^= ~(Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k]);
                    break;
                case 2:
                    if (j == 1)
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] = (Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k]) | 0x01;
                    }
                    else
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] = (Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k]);
                    }
                case 3:
                    if (((_usx + (_usy + j) * 192 + k) % 192 >= 1) && ((_usx + (_usy + j) * 192 + k) % 192 <= 192))
                    {
                        frame_buffer[_usx + (_usy + j) * 192 + k] ^= Chinese_text_16x16[i]._ucCHN_Code[j * 16 + k];
                    }
                    break;
                }
            }
        }
#else
#endif
    }
    else if (_ucSize == 2)
    { // 24x24字符
        __NOP();
    }
}

/**
 * @brief 绘制英文
 *
 * @param _usx
 * @param _usy
 * @param _pcStr
 * @param _ucSize
 * @param _ucMode
 */
static void vShowENGChar(uint16_t _usx, uint16_t _usy, uint8_t *_pcStr, uint8_t _ucSize, uint8_t _ucMode)
{
    uint8_t usASCIndex = 0;
    uint32_t i, j;

    usASCIndex = *_pcStr - ' ';
    if (usASCIndex > 94)
    { // ASCII位置超限
        return;
    }

    if (_ucSize == 0 || _ucSize == 1)
    { // 8x16字符
#ifdef JLX_SPIDMA
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 8; j++)
            {
                switch (_ucMode)
                {
                case 0:
                    frame_buffer[_usx + (_usy + i) * 192 + j] ^= english_text_8x16[(usASCIndex << 4) + (i << 3) + j];
                    break;
                case 1:
                    frame_buffer[_usx + (_usy + i) * 192 + j] ^= ~(english_text_8x16[(usASCIndex << 4) + (i << 3) + j]);
                    break;
                case 2:
                    if (i == 1)
                    {
                        frame_buffer[_usx + (_usy + i) * 192 + j] = english_text_8x16[(usASCIndex << 4) + (i << 3) + j] | 0x01;
                    }
                    else
                    {
                        frame_buffer[_usx + (_usy + i) * 192 + j] = english_text_8x16[(usASCIndex << 4) + (i << 3) + j];
                    }
                    break;
                case 3:
                    if (((_usx + (_usy + i) * 192 + j) % 192 >= 10) && ((_usx + (_usy + i) * 192 + j) % 192 <= 186))
                    {
                        frame_buffer[_usx + (_usy + i) * 192 + j] ^= english_text_8x16[(usASCIndex << 4) + (i << 3) + j];
                    }
                    break;
                }
            }
        }
#else
        vLcdSetCursor(_usx, _usy, 8, 2);
        for (i = 0; i < 16; i++)
        {
            if (!_ucMode)
            {
                vSpiSendData(english_text_8x16[usASCIndex * 16 + i]);
            }
            else
            {
                vSpiSendData(~(english_text_8x16[usASCIndex * 16 + i]));
            }
        }
#endif
    }
    else if (_ucSize == 2)
    { // 16x16字符
    }
}

/**
 * @brief (阻塞)设置绘制窗口
 *
 * @param sx
 * @param sy
 * @param lenx
 * @param leny
 */
static void vLcdSetCursor(uint8_t sx, uint8_t sy, uint8_t lenx, uint8_t leny)
{
    vSpiSendCmd(0x30);                // EXT=0
    vSpiSendCmd(0x75);                // Page Address setting
    vSpiSendData(sy);                 // XS=0
    vSpiSendData(sy + leny - 1);      // XE=159
    vSpiSendCmd(0x15);                // Clumn Address setting
    vSpiSendData(sx + 64);            // XS=0
    vSpiSendData(sx + lenx + 64 - 1); // XE=191
    vSpiSendCmd(0x5c);                // wirte gram
}

/**
 * @brief (阻塞)SPI发送数据
 *
 * @param byte
 */
static void vSpiSendData(uint8_t byte)
{
    JLX_SPI_CS(0); // 片选
    JLX_SPI_RS(1);

    /* loop while data register in not emplty */
    while (RESET == spi_i2s_flag_get(LCD_SPI_INTERFACE, SPI_STAT_TBE))
        ;

    /* send byte through the LCD_SPI_INTERFACE peripheral */
    spi_i2s_data_transmit(LCD_SPI_INTERFACE, byte);

    /* loop while data register in not emplty */
    while (SET == spi_i2s_flag_get(LCD_SPI_INTERFACE, SPI_STAT_TRANS))
        ;

    JLX_SPI_CS(1);
}

/**
 * @brief (阻塞)SPI发送命令
 *
 * @param byte
 */
static void vSpiSendCmd(uint8_t byte)
{
    JLX_SPI_CS(0); // 片选
    JLX_SPI_RS(0);
    /* loop while data register in not emplty */
    while (RESET == spi_i2s_flag_get(LCD_SPI_INTERFACE, SPI_STAT_TBE))
        ;

    /* send byte through the LCD_SPI_INTERFACE peripheral */
    spi_i2s_data_transmit(LCD_SPI_INTERFACE, byte);

    /* loop while data register in not emplty */
    while (SET == spi_i2s_flag_get(LCD_SPI_INTERFACE, SPI_STAT_TRANS))
        ;

    JLX_SPI_CS(1);
}
void bsp_JLXLcdShow(uint16_t _usx, uint16_t _usy, uint16_t _width, uint16_t _height, const char *_icon, uint8_t _ucMode)
{
    uint32_t i, j, z = 0;
    for (i = 0; i < _height && (_usy + i) < JLXLCD_H / 8; i++)
    {
        for (j = _usx; j < _width + _usx && j < JLXLCD_W; j++)
        {
            frame_buffer[(_usy + i) * 192 + j] |= _icon[z++];
        }
    }
}
void bsp_JLXLcdShowBottonIcon(const char *bottonIcon)
{
    if (bottonIcon == NULL)
        return; // 如果指针为空，直接返回
    memcpy(frame_buffer, bottonIcon, 192 * 16);
}
/*B_WL增加函数*/

