/**
 * ADS1292R 连接测试
 */

#include "main.h"
#include "ili9488.h"
#include "ads_connect_test.h"

extern SPI_HandleTypeDef hspi2;
volatile uint32_t dbg_stage = 0;

volatile uint8_t ads_id = 0;
volatile uint8_t ads_status = 0;
volatile uint8_t ads_done = 0;
volatile uint32_t ads_retry_count = 0;

#define CS_LOW()   HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET)
#define RST_LOW()  HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_RESET)
#define RST_HIGH() HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_SET)
/* START 未连接 */

uint8_t Read_ADS_ID(void) {
    uint8_t tx[3] = {0x20, 0x00, 0x00};  /* RREG addr=0 + count=1 + dummy */
    uint8_t rx[3] = {0};

    CS_LOW();
    HAL_Delay(1);
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 100);
    HAL_Delay(1);
    CS_HIGH();

    return rx[2];
}

void ADS1292R_ConnectTest(void) {
    uint8_t id;

    dbg_stage = 10;

    LCD_ShowTesting();
    dbg_stage = 11;

    /* 硬件复位 */
    RST_HIGH();
    HAL_Delay(50);
    RST_LOW();
    HAL_Delay(100);
    RST_HIGH();
    HAL_Delay(1000);  /* 等1秒让芯片完全稳定 */

    /* CS拉低，发8个dummy时钟同步SPI总线 */
    CS_LOW();
    HAL_Delay(2);
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 8; i++) {
        HAL_SPI_Transmit(&hspi2, &dummy, 1, 100);
    }
    HAL_Delay(2);
    CS_HIGH();
    HAL_Delay(10);

    /* 发送SDATAC停止连续读取 */
    CS_LOW();
    HAL_Delay(2);
    uint8_t sdatac = 0x11;
    HAL_SPI_Transmit(&hspi2, &sdatac, 1, 100);
    HAL_Delay(50);  /* 等50ms让SDATAC生效 */
    CS_HIGH();
    HAL_Delay(50);

    /* 尝试读ID，10次 */
    ads_status = 2;
    for (ads_retry_count = 0; ads_retry_count < 10; ads_retry_count++) {
        id = Read_ADS_ID();
        ads_id = id;
        if (id == 0x73 || id == 0x53) {
            ads_status = 1;
            break;
        }
        HAL_Delay(100);
    }

    ads_done = 1;
    dbg_stage = 20;
    LCD_ShowTestResult(ads_id, ads_status, ads_done, ads_retry_count);

    while (1) { __NOP(); }
}
