/**
  ******************************************************************************
  * @file    usb_dfu_media.c
  * @brief   DFU Flash media interface implementation for STM32G491
  ******************************************************************************
  */

#include "usb_dfu_media.h"
#include "usbd_dfu.h"
#include <string.h>

/* Private variables */

/* DFU media operations structure */
USBD_DFU_MediaTypeDef USBD_DFU_MEDIA_fops =
{
  (uint8_t *)"@Internal Flash  /0x08000000/04*016Kg,01*064Kg,07*128Kg",
  DFU_Media_Init,
  DFU_Media_DeInit,
  DFU_Media_Erase,
  DFU_Media_Write,
  DFU_Media_Read,
  DFU_Media_GetStatus,
};

/**
  * @brief  Initialize flash interface
  * @retval 0 if successful
  */
uint16_t DFU_Media_Init(void)
{
  /* Unlock the Flash to enable the flash control register access */
  HAL_FLASH_Unlock();
  return 0;
}

/**
  * @brief  De-initialize flash interface
  * @retval 0 if successful
  */
uint16_t DFU_Media_DeInit(void)
{
  /* Lock the Flash to disable the flash control register access */
  HAL_FLASH_Lock();
  return 0;
}

/**
  * @brief  Erase flash sector/page
  * @param  Add: Address of sector to be erased
  * @retval 0 if successful
  */
uint16_t DFU_Media_Erase(uint32_t Add)
{
  uint32_t PageError = 0;
  FLASH_EraseInitTypeDef EraseInit;
  
  /* Calculate page number */
  EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInit.Banks = FLASH_BANK_1;
  EraseInit.Page = (Add - FLASH_START_ADDR) / FLASH_PAGE_SIZE;
  EraseInit.NbPages = 1;
  
  if (HAL_FLASHEx_Erase(&EraseInit, &PageError) != HAL_OK)
  {
    return 1;
  }
  
  return 0;
}

/**
  * @brief  Write data to flash
  * @param  src: Pointer to source buffer
  * @param  dest: Pointer to destination address in flash
  * @param  Len: Number of bytes to write
  * @retval 0 if successful
  */
uint16_t DFU_Media_Write(uint8_t *src, uint8_t *dest, uint32_t Len)
{
  uint32_t i = 0;
  
  /* Program flash word by word (64-bit for STM32G4) */
  for (i = 0; i < Len; i += 8)
  {
    uint64_t data = 0;
    uint32_t remaining = Len - i;
    
    if (remaining >= 8)
    {
      memcpy(&data, src + i, 8);
    }
    else
    {
      /* Pad with 0xFF for last partial word */
      memset(&data, 0xFF, 8);
      memcpy(&data, src + i, remaining);
    }
    
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 
                          (uint32_t)(dest + i), data) != HAL_OK)
    {
      return 1;
    }
  }
  
  return 0;
}

/**
  * @brief  Read data from flash
  * @param  src: Pointer to source address in flash
  * @param  dest: Pointer to destination buffer
  * @param  Len: Number of bytes to read
  * @retval Pointer to destination buffer
  */
uint8_t *DFU_Media_Read(uint8_t *src, uint8_t *dest, uint32_t Len)
{
  memcpy(dest, src, Len);
  return dest;
}

/**
  * @brief  Get status for erase/program operations
  * @param  Add: Address being operated on
  * @param  Cmd: DFU_MEDIA_ERASE or DFU_MEDIA_PROGRAM
  * @param  buffer: Buffer for status info
  * @retval 0 if successful
  */
uint16_t DFU_Media_GetStatus(uint32_t Add, uint8_t Cmd, uint8_t *buffer)
{
  switch (Cmd)
  {
    case DFU_MEDIA_PROGRAM:
      /* Programming time: ~2ms per page at 128MHz */
      buffer[1] = 0;
      buffer[2] = 2;  /* 2ms */
      buffer[3] = 0;
      break;
      
    case DFU_MEDIA_ERASE:
    default:
      /* Erase time: ~20ms per page */
      buffer[1] = 0;
      buffer[2] = 20; /* 20ms */
      buffer[3] = 0;
      break;
  }
  return 0;
}
