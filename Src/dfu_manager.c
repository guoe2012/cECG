/**
  ******************************************************************************
  * @file    dfu_manager.c
  * @brief   DFU mode management - handles entry request and bootloader jump
  ******************************************************************************
  */

#include "dfu_manager.h"
#include "usbd_cdc_if.h"
#include <string.h>

/* Flag indicating DFU mode requested */
static volatile uint8_t dfu_requested = 0;

/* DFU flag placed in .noinit section (survives soft reset, NOT cleared by startup) */
volatile uint32_t dfu_flag_ram __attribute__((section(".noinit")));

/**
  * @brief  Initialize DFU manager
  */
void DFU_Init(void)
{
  /* No special init needed for RAM-based flag */
}

/**
  * @brief  Check if DFU mode was requested
  * @retval 1 if DFU requested, 0 otherwise
  */
uint8_t DFU_IsRequested(void)
{
  return (*DFU_FLAG_PTR == DFU_MAGIC_VALUE) ? 1 : 0;
}

/**
  * @brief  Request DFU mode entry
  */
void DFU_RequestEnter(void)
{
  dfu_requested = 1;
}

/**
  * @brief  Clear DFU request flag
  */
void DFU_ClearRequest(void)
{
  *DFU_FLAG_PTR = 0;
  dfu_requested = 0;
}

/**
  * @brief  Handle CDC command for DFU
  * @param  buf: Received data buffer
  * @param  len: Length of data
  */
void DFU_HandleCommand(uint8_t *buf, uint32_t len)
{
  /* Check for DFU command: "DFU\r\n" or "dfu\r\n" */
  if (len >= 3 && (buf[0] == 'D' || buf[0] == 'd') &&
      (buf[1] == 'F' || buf[1] == 'f') &&
      (buf[2] == 'U' || buf[2] == 'u'))
  {
    /* Send acknowledgment */
    CDC_Transmit_FS((uint8_t *)"Entering DFU mode...\r\n", 21);
    
    /* Small delay to ensure message is sent */
    HAL_Delay(100);
    
    /* Clean up and jump directly to bootloader */
    DFU_JumpToBootloader();
  }
}

/**
  * @brief  Jump to system bootloader (DFU mode)
  * @note   Thoroughly cleans up all peripherals before jumping.
  *         STM32G4 bootloader USB init fails if peripherals are left dirty.
  */
void DFU_JumpToBootloader(void)
{
  /* 1. Disable all NVIC interrupts and clear pending */
  for (uint32_t i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  /* 2. Disable SysTick */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  /* 3. Pull USB D+/D- low for 100ms so host detects disconnect */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(100);

  /* 4. Reset PA11/PA12 to floating input (let bootloader re-init USB) */
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* 5. Disable ALL peripheral clocks (APB1, APB2, AHB1, AHB2) */
  RCC->AHB1ENR = 0;
  RCC->AHB2ENR = 0;
  RCC->APB1ENR1 = 0;
  RCC->APB1ENR2 = 0;
  RCC->APB2ENR = 0;

  /* 6. Reset RCC to HSI defaults */
  RCC->CR |= RCC_CR_HSION;
  RCC->CFGR = 0;
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON);

  /* 7. Read bootloader entry point and jump directly (no SYSCFG remap needed) */
  uint32_t bootloader_sp = *((uint32_t *)0x1FFF0000);
  uint32_t bootloader_entry = *((uint32_t *)0x1FFF0004);

  __set_MSP(bootloader_sp);

  void (*bootloader_reset)(void) = (void (*)(void))bootloader_entry;
  bootloader_reset();

  while(1) {}
}
