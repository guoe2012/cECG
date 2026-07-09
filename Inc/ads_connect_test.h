#ifndef __ADS_CONNECT_TEST_H
#define __ADS_CONNECT_TEST_H

#include "main.h"

/* ADS1292R 引脚定义 */
#define ADS_CS_PIN     GPIO_PIN_12
#define ADS_CS_PORT    GPIOB
#define ADS_RST_PIN    GPIO_PIN_0
#define ADS_RST_PORT   GPIOC
#define ADS_DRDY_PIN   GPIO_PIN_10
#define ADS_DRDY_PORT  GPIOB
#define ADS_START_PIN  GPIO_PIN_1
#define ADS_START_PORT GPIOA

/* 外部变量 - 可在调试器中查看 */
extern volatile uint8_t ads_id;
extern volatile uint8_t ads_status;
extern volatile uint8_t ads_done;
extern volatile uint32_t ads_retry_count;

/* 函数声明 */
uint8_t Read_ADS_ID(void);
void ADS1292R_ConnectTest(void);

#endif
