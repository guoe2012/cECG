/**
 * ADS1292R 测试头文件
 */

#ifndef __ADS1292R_TEST_H
#define __ADS1292R_TEST_H

#include "main.h"

/* 外部变量 */
extern SPI_HandleTypeDef hspi2;

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

/* 引脚定义（与 ads_connect_test.h 保持一致） */
#define ADS_CS_PIN     GPIO_PIN_12
#define ADS_CS_PORT    GPIOB
#define ADS_RST_PIN    GPIO_PIN_0
#define ADS_RST_PORT   GPIOC
/* START 引脚已接地，由 SPI CMD_START/CMD_STOP 控制，不需要 GPIO 定义 */
#define ADS_DRDY_PIN   GPIO_PIN_10
#define ADS_DRDY_PORT  GPIOB

/* 函数声明 */
void ADS1292R_Reset(void);
void ADS1292R_SendCmd(uint8_t cmd);
uint8_t ADS1292R_ReadReg(uint8_t addr);
void ADS1292R_WriteReg(uint8_t addr, uint8_t data);
uint8_t ADS1292R_Test(void);

/* 测试函数 */
uint8_t Test_ReadID(void);
uint8_t Test_RegReadWrite(void);
uint8_t Test_DRDY(void);
uint8_t Test_ReadData(void);
uint8_t ADS1292R_RunAllTests(void);
void ADS1292R_TestMain(void);

#endif /* __ADS1292R_TEST_H */
