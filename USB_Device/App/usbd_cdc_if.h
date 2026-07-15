#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"
#include "usb_device.h"

/* 缓冲区大小 */
#define APP_RX_DATA_SIZE  256
#define APP_TX_DATA_SIZE  256

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);
void CDC_ReceiveCallback(uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
