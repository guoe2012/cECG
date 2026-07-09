/**
 * ECG 数据采集模块
 * ADS1292R 心电信号采集与LCD波形显示
 */

#include "main.h"
#include "ili9488.h"
#include "ecg_acquisition.h"
#include "ads_connect_test.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi2;

volatile int32_t ecg_buffer_ch1[ECG_BUFFER_SIZE];
volatile int32_t ecg_buffer_ch2[ECG_BUFFER_SIZE];
volatile uint32_t ecg_sample_count = 0;
volatile uint8_t ecg_data_ready = 0;
volatile int32_t dbg_ch1_raw = 0;
volatile int32_t dbg_ch2_raw = 0;
volatile uint8_t dbg_status[3] = {0};
volatile uint8_t dbg_regs[12] = {0};

static volatile uint32_t waveform_x = 0;

#define CS_LOW()   HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET)
#define RST_LOW()  HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_RESET)
#define RST_HIGH() HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_SET)
#define START_LOW()  HAL_GPIO_WritePin(ADS_START_PORT, ADS_START_PIN, GPIO_PIN_RESET)
#define START_HIGH() HAL_GPIO_WritePin(ADS_START_PORT, ADS_START_PIN, GPIO_PIN_SET)

static uint8_t ADS_ReadReg(uint8_t addr) {
    uint8_t tx[4] = {0xFF, ADS_CMD_RREG | addr, 0x00, 0x00};
    uint8_t rx[4] = {0};
    CS_LOW();
    HAL_Delay(2);
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, 100);
    HAL_Delay(2);
    CS_HIGH();
    return rx[3];
}

static void ADS_SendCmd(uint8_t cmd) {
    CS_LOW();
    HAL_Delay(2);
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2);
    CS_HIGH();
}

static void ADS_WriteReg(uint8_t addr, uint8_t data) {
    uint8_t cmd;
    CS_LOW();
    HAL_Delay(2);
    cmd = ADS_CMD_WREG | addr;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2);
    cmd = 0x00;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    HAL_Delay(2);
    HAL_SPI_Transmit(&hspi2, &data, 1, 100);
    HAL_Delay(2);
    CS_HIGH();
}

/* 修复1: 用 FillRect 替代逐像素绘制，大幅减少SPI事务次数 */
static void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    LCD_FillRect(x, y, w, 1, color);
}

static void LCD_DrawVLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color) {
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    LCD_FillRect(x, y1, 1, y2 - y1 + 1, color);
}

void ECG_Init(void) {
    RST_HIGH();
    HAL_Delay(50);
    RST_LOW();
    HAL_Delay(100);
    RST_HIGH();
    HAL_Delay(1000);

    CS_LOW();
    HAL_Delay(1);
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 8; i++) {
        HAL_SPI_Transmit(&hspi2, &dummy, 1, 100);
    }
    HAL_Delay(1);
    CS_HIGH();
    HAL_Delay(10);

    /* 发送RESET命令复位芯片内部状态机 */
    ADS_SendCmd(ADS_CMD_RESET);
    HAL_Delay(50);

    ADS_SendCmd(ADS_CMD_SDATAC);
    HAL_Delay(50);

    /* 回读ID验证SPI通信 */
    dbg_regs[0] = ADS_ReadReg(REG_ID);

    /* 方波测试模式: 使用 ADS1292R 内部测试信号验证数据通路 */
    ADS_WriteReg(REG_CONFIG1, 0x02);  /* 连续模式, 500 SPS */
    ADS_WriteReg(REG_CONFIG2, 0xA3);  /* 内部参考 ON + 方波测试信号 ON (1Hz) */
    ADS_WriteReg(REG_CH1SET, 0x65);   /* 增益6, MUX=101(内部测试信号) */
    ADS_WriteReg(REG_CH2SET, 0x65);   /* 增益6, MUX=101(内部测试信号) */
    ADS_WriteReg(REG_RLDSENS, 0x00);  /* 方波测试时关闭RLD */
    ADS_WriteReg(REG_LOFFSENS, 0x00); /* 关闭脱落检测 */
    HAL_Delay(10);

    /* 回读寄存器验证写入 */
    dbg_regs[1] = ADS_ReadReg(REG_CONFIG1);
    dbg_regs[2] = ADS_ReadReg(REG_CONFIG2); /* 期望 0xA3 */
    dbg_regs[3] = ADS_ReadReg(REG_CH1SET);  /* 期望 0x65 */
    dbg_regs[4] = ADS_ReadReg(REG_CH2SET);  /* 期望 0x65 */
}

void ECG_Start(void) {
    /* STOP -> START -> RDATAC 标准连续转换序列 */
    ADS_SendCmd(ADS_CMD_STOP);
    HAL_Delay(2);
    ADS_SendCmd(ADS_CMD_START);
    HAL_Delay(10);
    ADS_SendCmd(ADS_CMD_RDATAC);
    HAL_Delay(10);
}

void ECG_Stop(void) {
    ADS_SendCmd(ADS_CMD_SDATAC);
    ADS_SendCmd(ADS_CMD_STOP);
}

uint8_t ECG_ReadData(int32_t *ch1, int32_t *ch2) {
    uint8_t tx[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t rx[9] = {0};

    HAL_Delay(2);

    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 9, 200);
    CS_HIGH();

    *ch1 = (int32_t)(((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3]);
    if (rx[1] & 0x80) *ch1 |= 0xFF000000;

    *ch2 = (int32_t)(((uint32_t)rx[4] << 16) | ((uint32_t)rx[5] << 8) | rx[6]);
    if (rx[4] & 0x80) *ch2 |= 0xFF000000;

    dbg_ch1_raw = *ch1;
    dbg_ch2_raw = *ch2;
    dbg_status[0] = rx[0];

    return 1;
}

/* 修复2: 自适应缩放
 * ADS1292R: VREF=2.4V, 增益=6, 满量程=VREF/Gain=0.4V
 * 24bit有符号满量程: ±0x7FFFFF ≈ ±8388607
 * 每像素对应 = (2 * 8388607) / height 个LSB
 */
static int16_t ECG_ScaleToY(int32_t raw, uint16_t center_y, uint16_t height) {
    /* 假设内部测试信号幅值约为 20000 LSB，我们希望它显示为约 40 像素高度 */
    /* 缩放因子：y = center_y - (raw / 16384) */
    int16_t scaled = (int16_t)(raw / 16384);
    int16_t y = center_y - scaled;
    int16_t half_h = (int16_t)(height / 2);
    if (y < (int16_t)(center_y - half_h)) y = (int16_t)(center_y - half_h);
    if (y > (int16_t)(center_y + half_h - 1)) y = (int16_t)(center_y + half_h - 1);
    return y;
}

void ECG_DisplayWaveform(void) {
    uint16_t ch1_center = 125;
    uint16_t ch2_center = 255;

    LCD_FillScreen(COLOR_BLACK);

    LCD_DrawString(10, 5, "ADS1292R ECG  CH1:", COLOR_CYAN, COLOR_BLACK, 2);
    LCD_DrawString(10, 165, "ADS1292R ECG  CH2:", COLOR_YELLOW, COLOR_BLACK, 2);

    LCD_DrawHLine(0, ch1_center, LCD_WIDTH, COLOR_DARKGRAY);
    LCD_DrawHLine(0, ch2_center, LCD_WIDTH, COLOR_DARKGRAY);
}

void ECG_Run(void) {
    int32_t ch1, ch2;
    int16_t y1, y2;
    uint16_t ch1_center = 125;
    uint16_t ch2_center = 255;
    uint16_t ch_height = 130;
    int16_t prev_y1 = ch1_center;
    int16_t prev_y2 = ch2_center;
    uint32_t display_update_count = 0;
    char buf[32];

    /* 诊断: 基线漂移跟踪 */
    int32_t ch1_baseline = 0;
    int32_t ch2_baseline = 0;

    ECG_Init();
    LCD_Init();
    ECG_DisplayWaveform();
    ECG_Start();

    waveform_x = 0;
    ecg_sample_count = 0;

    while (1) {
        if (!ECG_ReadData(&ch1, &ch2)) continue;

        ecg_buffer_ch1[ecg_sample_count % ECG_BUFFER_SIZE] = ch1;
        ecg_buffer_ch2[ecg_sample_count % ECG_BUFFER_SIZE] = ch2;
        ecg_sample_count++;

        /* 直接用原始数据，去掉滤波 */
        y1 = ECG_ScaleToY(ch1, ch1_center, ch_height);
        y2 = ECG_ScaleToY(ch2, ch2_center, ch_height);

        /* 只在每4个采样点更新一次LCD */
        if (ecg_sample_count % 4 == 0) {
            /* 擦除旧列 */
            LCD_FillRect(waveform_x, ch1_center - ch_height / 2, 1, ch_height, COLOR_BLACK);
            LCD_FillRect(waveform_x, ch2_center - ch_height / 2, 1, ch_height, COLOR_BLACK);

            /* 画网格线 */
            LCD_DrawVLine(waveform_x, ch1_center - ch_height / 2, ch1_center + ch_height / 2 - 1, 0x1082);
            LCD_DrawVLine(waveform_x, ch2_center - ch_height / 2, ch2_center + ch_height / 2 - 1, 0x1082);

            /* 画新点 */
            LCD_DrawPixel(waveform_x, y1, COLOR_CYAN);
            LCD_DrawPixel(waveform_x, y2, COLOR_YELLOW);

            /* 连线 */
            if (waveform_x > 0) {
                LCD_DrawVLine(waveform_x - 1, prev_y1, y1, COLOR_CYAN);
                LCD_DrawVLine(waveform_x - 1, prev_y2, y2, COLOR_YELLOW);
            }

            prev_y1 = y1;
            prev_y2 = y2;
            waveform_x++;
            /* 修复3: 回卷时重置前一帧Y坐标，避免从屏幕右端到左端画出跨屏竖线 */
            if (waveform_x >= LCD_WIDTH) {
                waveform_x = 0;
                prev_y1 = ch1_center;
                prev_y2 = ch2_center;
            }
        }

        /* 每100个采样更新文字 */
        display_update_count++;
        if (display_update_count >= 100) {
            display_update_count = 0;
            sprintf(buf, "N:%lu", (unsigned long)ecg_sample_count);
            LCD_DrawString(350, 5, buf, COLOR_WHITE, COLOR_BLACK, 2);
            sprintf(buf, "C1:%ld  ", (long)ch1);
            LCD_DrawString(350, 165, buf, COLOR_WHITE, COLOR_BLACK, 2);
        }
    }
}
