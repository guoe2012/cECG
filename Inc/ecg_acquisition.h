/**
 * ECG 数据采集模块
 * ADS1292R 心电信号采集与LCD波形显示
 */

#ifndef __ECG_ACQUISITION_H
#define __ECG_ACQUISITION_H

#include "main.h"

/* ADS1292R 命令 */
#define ADS_CMD_WAKEUP    0x02
#define ADS_CMD_STANDBY   0x04
#define ADS_CMD_RESET     0x06
#define ADS_CMD_START     0x08
#define ADS_CMD_STOP      0x0A
#define ADS_CMD_RDATAC    0x10
#define ADS_CMD_SDATAC    0x11
#define ADS_CMD_RDATA     0x12
#define ADS_CMD_RREG      0x20
#define ADS_CMD_WREG      0x40

/* ADS1292R 寄存器 */
#define REG_ID       0x00
#define REG_CONFIG1  0x01
#define REG_CONFIG2  0x02
#define REG_LOFF     0x03
#define REG_CH1SET   0x04
#define REG_CH2SET   0x05
#define REG_RLDSENS  0x06
#define REG_LOFFSENS 0x07
#define REG_LOFFSTAT 0x08
#define REG_RESP1    0x09
#define REG_RESP2    0x0A
#define REG_GPIO     0x0B

/* ECG 数据缓冲区大小 */
#define ECG_BUFFER_SIZE  480  /* 一个屏幕宽度的采样点 */

/* 外部变量 */
extern volatile int32_t ecg_buffer_ch1[ECG_BUFFER_SIZE];
extern volatile int32_t ecg_buffer_ch2[ECG_BUFFER_SIZE];
extern volatile uint32_t ecg_sample_count;
extern volatile uint8_t ecg_data_ready;

/* 函数声明 */
void ECG_Init(void);
void ECG_Start(void);
void ECG_Stop(void);
uint8_t ECG_ReadData(int32_t *ch1, int32_t *ch2);
void ECG_DisplayWaveform(void);
void ECG_Run(void);

#endif /* __ECG_ACQUISITION_H */
