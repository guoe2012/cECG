#include "main.h"
#include "ili9488.h"
#include "ads_connect_test.h"
#include "ads1292r_test.h"
#include "ecg_acquisition.h"

SPI_HandleTypeDef hspi2;
volatile uint32_t dbg_cfgr = 0;

static void My_HAL_Init(void)
{
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  SysTick_Config(SystemCoreClock / 1000U);
  __enable_irq();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* PA8 = MCO输出，为ADS1292R提供时钟 (HSI=16MHz /8 = 2MHz) */
  RCC->CFGR &= ~(RCC_CFGR_MCOSEL | RCC_CFGR_MCOPRE);
  RCC->CFGR |= (RCC_CFGR_MCOSEL_0 | RCC_CFGR_MCOSEL_1 | RCC_CFGR_MCOPRE_0 | RCC_CFGR_MCOPRE_1);
  dbg_cfgr = RCC->CFGR;
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC8 = LCD背光 */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PC13 = LED 指示灯（连接测试结果），默认灭（高电平） */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PB12 = ADS_CS 默认高 */
  HAL_GPIO_WritePin(ADS_CS_PORT, ADS_CS_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = ADS_CS_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(ADS_CS_PORT, &GPIO_InitStruct);

  /* PC0 = ADS_RST 默认高 */
  HAL_GPIO_WritePin(ADS_RST_PORT, ADS_RST_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = ADS_RST_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ADS_RST_PORT, &GPIO_InitStruct);

  /* PA1 = ADS_START 已硬件接地，不初始化 GPIO 输出（避免驱动短路） */
  /* START 由 SPI CMD_START / CMD_STOP 命令控制 */

  /* PB10 = ADS_DRDY 输入上拉 */
  GPIO_InitStruct.Pin = ADS_DRDY_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ADS_DRDY_PORT, &GPIO_InitStruct);
}

static void MX_SPI2_Init(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;     /* CPOL=0 */
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;         /* CPHA=1, SPI Mode 1 */
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 2MHz (16MHz / 8) */
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

int main(void)
{
  My_HAL_Init();
  MX_GPIO_Init();
  MX_SPI2_Init();

  /* === 方波测试模式 ===
   * CONFIG2=0xA3 开启内部方波，CH1/CH2 MUX=101（内部测试信号）
   * LCD 上应显示 1Hz 方波
   */
  ECG_Run();   /* 方波模式入口 */

  /* === 连接测试（注释掉以启用）===
  ADS1292R_TestMain();
  */

  while (1) {}
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
