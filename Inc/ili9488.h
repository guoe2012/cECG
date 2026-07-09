/**
 * ILI9488 LCD 驱动头文件
 * 屏幕: 3.5寸 480x320 TFT (SPI接口)
 * SPI外设: SPI3 (PB3=SCK, PB5=MOSI)
 * 控制引脚: PA15=CS, PC7=DC, PC8=RST/背光
 *
 * 注意: ILI9488 SPI模式只支持 RGB666(18bit) 像素格式
 *       每个像素需要3字节传输(R[7:2], G[7:2], B[7:2])
 *       驱动内部自动将RGB565转换为RGB666发送
 */

#ifndef __ILI9488_H
#define __ILI9488_H

#include "main.h"

/* ============ 引脚定义 ============ */
#define LCD_CS_PIN      GPIO_PIN_15
#define LCD_CS_PORT     GPIOA
#define LCD_DC_PIN      GPIO_PIN_6
#define LCD_DC_PORT     GPIOC
#define LCD_RST_PIN     GPIO_PIN_7
#define LCD_RST_PORT    GPIOC
/* 背光控制：PC8（独立引脚） */
#define LCD_BL_PIN      GPIO_PIN_8
#define LCD_BL_PORT     GPIOC

/* ============ 屏幕尺寸（修正：480x320 横屏） ============ */
#define LCD_WIDTH       480
#define LCD_HEIGHT      320

/* ============ RGB565 颜色（内部转换为RGB666发送） ============ */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFD20
#define COLOR_DARKGRAY  0x4208
#define COLOR_LIGHTGRAY 0xC618
#define COLOR_DARKGREEN 0x03E0

/* ============ ILI9488 命令 ============ */
#define ILI9488_NOP         0x00
#define ILI9488_SWRESET     0x01
#define ILI9488_SLPIN       0x10
#define ILI9488_SLPOUT      0x11
#define ILI9488_NORON       0x13
#define ILI9488_INVOFF      0x20
#define ILI9488_INVON       0x21
#define ILI9488_DISPOFF     0x28
#define ILI9488_DISPON      0x29
#define ILI9488_CASET       0x2A
#define ILI9488_RASET       0x2B
#define ILI9488_RAMWR       0x2C
#define ILI9488_MADCTL      0x36
#define ILI9488_COLMOD      0x3A
#define ILI9488_IFMODE      0xB0
#define ILI9488_FRMCTR1     0xB1
#define ILI9488_INVCTR      0xB4
#define ILI9488_DFUNCTR     0xB6
#define ILI9488_EMSET       0xB7
#define ILI9488_PWCTR1      0xC0
#define ILI9488_PWCTR2      0xC1
#define ILI9488_VMCTR1      0xC5
#define ILI9488_PGAMCTRL    0xE0
#define ILI9488_NGAMCTRL    0xE1
#define ILI9488_ADJCTR3     0xF7

/* MADCTL 位定义 */
#define MADCTL_MY   0x80
#define MADCTL_MX   0x40
#define MADCTL_MV   0x20
#define MADCTL_ML   0x10
#define MADCTL_BGR  0x08
#define MADCTL_MH   0x04

/* ========== 底层SPI操作（内部使用，不对外暴露） ========== */

/* ========== RGB565→RGB666 转换（内部使用） ========== */

/* ============ 背光控制 ============ */

void LCD_Backlight_On(void);
void LCD_Backlight_Off(void);

/* ============ 初始化 & 基本绘图 ============ */

void LCD_Init(void);
void LCD_FillScreen(uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/* ============ 文字（内置5x7字体，size为放大倍数） ============ */

void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

/* ============ ADS1292R 测试结果显示 ============ */

void LCD_ShowTesting(void);
void LCD_ShowTestResult(uint8_t id, uint8_t status, uint8_t done, uint32_t retry);

#endif /* __ILI9488_H */
