#ifndef __USB_DFU_MEDIA_H
#define __USB_DFU_MEDIA_H

#include "usbd_dfu.h"
#include "stm32g4xx_hal.h"

/* Flash memory layout for STM32G491 */
#define FLASH_START_ADDR        0x08000000

/* Application starts after bootloader (if any) - currently at start of flash */
#define APP_DEFAULT_ADD         0x08000000

/* DFU transfer size */
#define DFU_XFER_SIZE           1024

/* Exported functions */
uint16_t DFU_Media_Init(void);
uint16_t DFU_Media_DeInit(void);
uint16_t DFU_Media_Erase(uint32_t Add);
uint16_t DFU_Media_Write(uint8_t *src, uint8_t *dest, uint32_t Len);
uint8_t *DFU_Media_Read(uint8_t *src, uint8_t *dest, uint32_t Len);
uint16_t DFU_Media_GetStatus(uint32_t Add, uint8_t Cmd, uint8_t *buffer);

/* DFU media operations structure */
extern USBD_DFU_MediaTypeDef USBD_DFU_MEDIA_fops;

#endif /* __USB_DFU_MEDIA_H */
