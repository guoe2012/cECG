/**
 * ILI9488 LCD 驱动实现
 * 3.5寸 480x320 TFT (SPI接口)
 *
 * SPI3: PB3=SCK(AF6), PB5=MOSI(AF6)
 * GPIO: PA15=CS, PC7=DC, PC8=RST
 *
 * ILI9488在SPI模式下只支持RGB666(18bit)像素格式，
 * 每像素需要发送3字节(R[7:2], G[7:2], B[7:2])。
 * 驱动内部将RGB565自动转换为RGB666发送。
 */

#include "ili9488.h"
#include <string.h>
#include <stdio.h>

/* SPI3句柄 */
static SPI_HandleTypeDef hspi3;

/* ============ 5x7 ASCII 字体 ============ */
/* 每个字符5列，每列7bit（低位在上），共95个可打印字符(0x20-0x7E) */
static const uint8_t Font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20 空格 */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x00,0x7F,0x10,0x28,0x44}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x04,0x08,0x10,0x08}, /* ~ */
};

/* ============ 底层SPI/GPIO操作 ============ */

#define LCD_CS_LOW()    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_DC_CMD()    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_DATA()   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define LCD_RST_LOW()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

/**
 * SPI3 + LCD GPIO 初始化
 */
static void LCD_SPI_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();

    /* 禁用JTAG，释放PA15/PB3给GPIO/SPI使用（保留SWD调试） */
    /* SYSCFG_CFGR1 SWJ_CFG[26:24] = 001: JTAG disable, SWD enable */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~(0x7UL << 24)) | (0x1UL << 24);

    /* PA15 = LCD_CS (输出，默认高) */
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LCD_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_CS_PORT, &GPIO_InitStruct);

    /* PC6 = LCD_DC (输出) */
    GPIO_InitStruct.Pin = LCD_DC_PIN;
    HAL_GPIO_Init(LCD_DC_PORT, &GPIO_InitStruct);

    /* PC7 = LCD_RST (输出，默认高) */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LCD_RST_PIN;
    HAL_GPIO_Init(LCD_RST_PORT, &GPIO_InitStruct);

    /* PC8 = LCD_BL (输出，默认开) */
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LCD_BL_PIN;
    HAL_GPIO_Init(LCD_BL_PORT, &GPIO_InitStruct);

    /* PB3 = SPI3_SCK, PB5 = SPI3_MOSI (AF6) */
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* SPI3 配置 - 最稳妥的全双工模式，1MHz */
    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_HIGH;   /* CPOL=1 */
    hspi3.Init.CLKPhase = SPI_PHASE_2EDGE;        /* CPHA=1, SPI Mode 3 */
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* 修复6: 实际速率取决于系统时钟，预分频/16（如系统时钟16MHz则1MHz） */
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;

    if (HAL_SPI_Init(&hspi3) != HAL_OK) {
        /* SPI初始化失败 - 保持背光亮表示错误 */
        LCD_Backlight_On();
        while (1);
    }
}

/* ========== 底层SPI发送（CS由调用者控制） ========== */

static void LCD_Send(uint8_t *buf, uint16_t len) {
    HAL_SPI_Transmit(&hspi3, buf, len, 1000);
}

/**
 * 发送命令（带参数）
 * cmd: 命令字节
 * data: 参数数组（可以为NULL）
 * len: 参数长度
 */
static void LCD_WriteCmd(uint8_t cmd, uint8_t *data, uint16_t len) {
    LCD_CS_LOW();
    LCD_DC_CMD();
    LCD_Send(&cmd, 1);
    if (data && len > 0) {
        LCD_DC_DATA();
        LCD_Send(data, len);
    }
    LCD_CS_HIGH();
}

/* ============ 背光控制 ============ */

#define LCD_BL_HIGH()  HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define LCD_BL_LOW()   HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET)

void LCD_Backlight_On(void) {
    LCD_BL_HIGH();
}

void LCD_Backlight_Off(void) {
    LCD_BL_LOW();
}

/* ============ RGB565 → RGB666 转换 ============ */

void LCD_RGB565_To_RGB666(uint16_t rgb565, uint8_t rgb666[3]) {
    rgb666[0] = (rgb565 >> 8) & 0xF8;
    rgb666[1] = (rgb565 >> 3) & 0xFC;
    rgb666[2] = (rgb565 << 3) & 0xF8;
}

/* ============ ILI9488 初始化 ============ */

/**
 * ILI9488 硬件复位 + 初始化序列
 */
void LCD_Init(void) {
    uint8_t data[16];

    /* 先初始化GPIO（不含SPI），立即点亮背光 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC8 = BL 推挽输出，立即点亮 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LCD_BL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_BL_PORT, &GPIO_InitStruct);
    HAL_Delay(50);  /* 背光稳定 */

    /* 再初始化SPI和其他引脚 */
    LCD_SPI_Init();

    /* === 硬件复位 === */
    LCD_RST_HIGH();
    HAL_Delay(10);
    LCD_RST_LOW();
    HAL_Delay(20);
    LCD_RST_HIGH();
    HAL_Delay(150);

    /* === SPI同步：发几个dummy字节，防止总线错位 === */
    LCD_CS_LOW();
    LCD_DC_CMD();
    data[0] = 0x00;
    for (int i = 0; i < 4; i++) LCD_Send(data, 1);
    LCD_CS_HIGH();
    HAL_Delay(10);

    /* === 软复位 === */
    LCD_WriteCmd(ILI9488_SWRESET, NULL, 0);
    HAL_Delay(150);

    /* === 退出睡眠 === */
    LCD_WriteCmd(ILI9488_SLPOUT, NULL, 0);
    HAL_Delay(150);

    /* === 像素格式: 0x66 = 18bit/pixel (RGB666) === */
    data[0] = 0x66;
    LCD_WriteCmd(ILI9488_COLMOD, data, 1);

    /* === MADCTL: 显示方向 - 横屏 (MV=1, MX=1, BGR=1) === */
    data[0] = MADCTL_MV | MADCTL_MX | MADCTL_BGR;  // 0x20|0x40|0x08 = 0x68
    LCD_WriteCmd(ILI9488_MADCTL, data, 1);

    /* === 接口模式控制 === */
    data[0] = 0x00;
    LCD_WriteCmd(ILI9488_IFMODE, data, 1);

    /* === 帧率控制 === */
    data[0] = 0xA0; data[1] = 0x11;
    LCD_WriteCmd(ILI9488_FRMCTR1, data, 2);

    /* === 反转控制 === */
    data[0] = 0x02;
    LCD_WriteCmd(ILI9488_INVCTR, data, 1);

    /* === 显示功能控制 === */
    data[0] = 0x02; data[1] = 0x22;
    LCD_WriteCmd(ILI9488_DFUNCTR, data, 2);

    /* === Entry Mode Set === */
    data[0] = 0xC6;
    LCD_WriteCmd(ILI9488_EMSET, data, 1);

    /* === 电源控制1 === */
    data[0] = 0x17; data[1] = 0x15;
    LCD_WriteCmd(ILI9488_PWCTR1, data, 2);

    /* === 电源控制2 === */
    data[0] = 0x41;
    LCD_WriteCmd(ILI9488_PWCTR2, data, 1);

    /* === VCOM控制 === */
    data[0] = 0x00; data[1] = 0x12; data[2] = 0x80;
    LCD_WriteCmd(ILI9488_VMCTR1, data, 3);

    /* === 正伽马 === */
    {
        uint8_t pgam[] = {0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
                          0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F};
        LCD_WriteCmd(ILI9488_PGAMCTRL, pgam, sizeof(pgam));
    }

    /* === 负伽马 === */
    {
        uint8_t ngam[] = {0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
                          0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
        LCD_WriteCmd(ILI9488_NGAMCTRL, ngam, sizeof(ngam));
    }

    /* === 调整控制3 === */
    data[0] = 0xA9; data[1] = 0x51; data[2] = 0x2C; data[3] = 0x82;
    LCD_WriteCmd(ILI9488_ADJCTR3, data, 4);

    /* === 关闭反色 === */
    LCD_WriteCmd(ILI9488_INVOFF, NULL, 0);

    /* === 正常显示模式 === */
    LCD_WriteCmd(ILI9488_NORON, NULL, 0);
    HAL_Delay(10);

    /* === 开显示 === */
    LCD_WriteCmd(ILI9488_DISPON, NULL, 0);
    HAL_Delay(100);

    /* === 开背光 === */
    LCD_Backlight_On();

    /* === 清屏为黑色 === */
    LCD_FillScreen(COLOR_BLACK);
}

/* ============ 基本绘图 ============ */

/**
 * 设置绘图窗口并发送 RAMWR 命令，CS保持低电平等待后续像素数据
 * 修复5: 将 RAMWR 命令与像素数据合并在同一个CS片选事务中，
 *         避免多余的CS上下沿引起时序问题。
 * 调用者必须在像素数据发送完毕后调用 LCD_CS_HIGH()。
 */
static void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    data[0] = x0 >> 8; data[1] = x0 & 0xFF;
    data[2] = x1 >> 8; data[3] = x1 & 0xFF;
    LCD_WriteCmd(ILI9488_CASET, data, 4);

    data[0] = y0 >> 8; data[1] = y0 & 0xFF;
    data[2] = y1 >> 8; data[3] = y1 & 0xFF;
    LCD_WriteCmd(ILI9488_RASET, data, 4);

    /* RAMWR 命令，之后保持CS低电平发送像素数据 */
    LCD_WriteCmd(ILI9488_RAMWR, NULL, 0);
}

/**
 * 填充矩形区域
 */
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);

    /* 预计算RGB666 */
    uint8_t rgb[3];
    rgb[0] = (color >> 8) & 0xF8;
    rgb[1] = (color >> 3) & 0xFC;
    rgb[2] = (color << 3) & 0xF8;

    /* 构建64像素的缓冲区 */
    #define FILL_BUF_PIXELS 64
    uint8_t buf[FILL_BUF_PIXELS * 3];
    for (uint16_t i = 0; i < FILL_BUF_PIXELS; i++) {
        buf[i * 3 + 0] = rgb[0];
        buf[i * 3 + 1] = rgb[1];
        buf[i * 3 + 2] = rgb[2];
    }

    uint32_t total = (uint32_t)w * h;
    LCD_DC_DATA();
    LCD_CS_LOW();
    while (total > 0) {
        uint16_t chunk = (total > FILL_BUF_PIXELS) ? FILL_BUF_PIXELS : total;
        LCD_Send(buf, chunk * 3);
        total -= chunk;
    }
    LCD_CS_HIGH();
}

/**
 * 画单个像素
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    LCD_SetWindow(x, y, x, y);

    uint8_t rgb[3];
    rgb[0] = (color >> 8) & 0xF8;
    rgb[1] = (color >> 3) & 0xFC;
    rgb[2] = (color << 3) & 0xF8;

    LCD_DC_DATA();
    LCD_CS_LOW();
    LCD_Send(rgb, 3);
    LCD_CS_HIGH();
}

/**
 * 全屏填充
 * 横屏模式: 宽480 x 高320
 */
void LCD_FillScreen(uint16_t color) {
    LCD_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

/* ============ 文字绘制 ============ */

/**
 * 绘制单个字符
 * size: 放大倍数 (1=5x7, 2=10x14, 3=15x21 ...)
 */
void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 0x20 || c > 0x7E) c = '?';

    const uint8_t *glyph = Font5x7[c - 0x20];

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            uint16_t px_color = (line & (1 << row)) ? color : bg;
            if (size == 1) {
                LCD_DrawPixel(x + col, y + row, px_color);
            } else {
                LCD_FillRect(x + col * size, y + row * size, size, size, px_color);
            }
        }
    }
    /* 字符间距（1列背景色） */
    if (size == 1) {
        for (uint8_t row = 0; row < 7; row++)
            LCD_DrawPixel(x + 5, y + row, bg);
    } else {
        LCD_FillRect(x + 5 * size, y, size, 7 * size, bg);
    }
}

/**
 * 绘制字符串
 */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    uint16_t x0 = x;
    uint8_t char_w = 6 * size;  /* 5像素+1间距 */
    uint8_t char_h = 7 * size;

    while (*str) {
        if (*str == '\n') {
            x = x0;
            y += char_h + 2;
            str++;
            continue;
        }
        if (x + char_w > LCD_WIDTH) {
            x = x0;
            y += char_h + 2;
        }
        LCD_DrawChar(x, y, *str, color, bg, size);
        x += char_w;
        str++;
    }
}

/* ============ ADS1292R 测试结果显示 ============ */

/**
 * 简单的uint8转十六进制字符串
 */
static void u8_to_hex(uint8_t val, char *buf) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(val >> 4) & 0x0F];
    buf[3] = hex[val & 0x0F];
    buf[4] = '\0';
}

/**
 * 简单的uint32转十进制字符串
 */
static void u32_to_dec(uint32_t val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[11];
    int i = 0;
    while (val > 0 && i < 10) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = 0; j < i; j++) {
        buf[j] = tmp[i - 1 - j];
    }
    buf[i] = '\0';
}

/**
 * 显示 "Testing..." 等待界面
 */
void LCD_ShowTesting(void) {
    LCD_FillScreen(COLOR_BLACK);

    /* 标题 */
    LCD_DrawString(20, 20, "ADS1292R SPI Test", COLOR_CYAN, COLOR_BLACK, 3);

    /* 分割线 */
    LCD_FillRect(20, 55, 440, 2, COLOR_DARKGRAY);

    /* 测试中提示 */
    LCD_DrawString(20, 80, "Testing...", COLOR_YELLOW, COLOR_BLACK, 3);

    LCD_DrawString(20, 140, "Reading chip ID...", COLOR_LIGHTGRAY, COLOR_BLACK, 2);
}

/**
 * 显示测试结果
 */
void LCD_ShowTestResult(uint8_t id, uint8_t status, uint8_t done, uint32_t retry) {
    char buf[32];

    LCD_FillScreen(COLOR_BLACK);

    /* ====== 标题 ====== */
    LCD_DrawString(20, 15, "ADS1292R SPI Test", COLOR_CYAN, COLOR_BLACK, 3);

    /* 分割线 */
    LCD_FillRect(20, 50, 440, 2, COLOR_DARKGRAY);

    /* ====== 芯片ID ====== */
    LCD_DrawString(20, 70, "Chip ID:", COLOR_WHITE, COLOR_BLACK, 2);
    u8_to_hex(id, buf);
    LCD_DrawString(140, 70, buf, COLOR_YELLOW, COLOR_BLACK, 2);

    /* 识别芯片型号 */
    if ((id & 0x0F) == 0x03) {
        LCD_DrawString(210, 70, "(ADS1292R)", COLOR_GREEN, COLOR_BLACK, 2);
    } else if ((id & 0x0F) == 0x01) {
        LCD_DrawString(210, 70, "(ADS1292)", COLOR_GREEN, COLOR_BLACK, 2);
    } else {
        LCD_DrawString(210, 70, "(Unknown)", COLOR_RED, COLOR_BLACK, 2);
    }

    /* ====== 状态 ====== */
    LCD_DrawString(20, 105, "Status:", COLOR_WHITE, COLOR_BLACK, 2);

    if (!done) {
        LCD_DrawString(120, 105, "RUNNING", COLOR_YELLOW, COLOR_BLACK, 3);
    } else if (status == 1) {
        /* 通过 - 大号绿色文字 */
        LCD_DrawString(120, 100, "PASS", COLOR_GREEN, COLOR_BLACK, 4);
        /* 绿色指示条 */
        LCD_FillRect(20, 145, 440, 4, COLOR_GREEN);
    } else {
        /* 失败 - 大号红色文字 */
        LCD_DrawString(120, 100, "FAIL", COLOR_RED, COLOR_BLACK, 4);
        /* 红色指示条 */
        LCD_FillRect(20, 145, 440, 4, COLOR_RED);
    }

    /* ====== 重试次数 ====== */
    LCD_DrawString(20, 165, "Retries:", COLOR_WHITE, COLOR_BLACK, 2);
    u32_to_dec(retry, buf);
    LCD_DrawString(140, 165, buf, COLOR_LIGHTGRAY, COLOR_BLACK, 2);

    /* ====== 详细信息 ====== */
    LCD_FillRect(20, 195, 440, 1, COLOR_DARKGRAY);

    LCD_DrawString(20, 210, "SPI Config:", COLOR_LIGHTGRAY, COLOR_BLACK, 2);
    /* 修复4: 实际SPI3配置为 Mode 3 (CPOL=1, CPHA=1) */
    LCD_DrawString(20, 235, " Mode 3, CPOL=1 CPHA=1", COLOR_DARKGRAY, COLOR_BLACK, 2);
    LCD_DrawString(20, 260, " Speed: /16 of SPI clk", COLOR_DARKGRAY, COLOR_BLACK, 2);

    LCD_DrawString(20, 290, "Pins: SCK=PB3 MOSI=PB5", COLOR_DARKGRAY, COLOR_BLACK, 2);
}
