/* 简化版 USB CDC 接口 —— 纯内存收发，不依赖 UART */
#include "usbd_cdc_if.h"
#include "dfu_manager.h"
#include "ads1292r_test.h"
#include <string.h>
#include <stdio.h>

/* 2位十六进制转字节 */
static uint8_t hex2byte(const uint8_t *hex) {
    uint8_t val = 0;
    for (int i = 0; i < 2; i++) {
        uint8_t c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= c - '0';
        else if (c >= 'A' && c <= 'F') val |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val |= c - 'a' + 10;
    }
    return val;
}

/* 接收/发送缓冲区 */
#define APP_RX_DATA_SIZE  256
#define APP_TX_DATA_SIZE  256

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* CDC 接口句柄 */
USBD_CDC_LineCodingTypeDef LineCoding = {
    115200, 0, 0, 8  /* bitrate, format=1stop, parity=none, datatype=8 */
};

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);

/* 回调接口 */
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    NULL  /* TransmitCplt */
};

/* 接收完成回调（弱定义，用户可在其他文件覆盖） */
__attribute__((weak)) void CDC_ReceiveCallback(uint8_t *buf, uint32_t len) {
    /* Check for DFU command first */
    if (len >= 3 && (buf[0] == 'D' || buf[0] == 'd') &&
        (buf[1] == 'F' || buf[1] == 'f') &&
        (buf[2] == 'U' || buf[2] == 'u'))
    {
        DFU_HandleCommand(buf, len);
        return;
    }

    /* 读寄存器: "Rxx\n" (xx = 2位十六进制地址) */
    if (len >= 3 && (buf[0] == 'R' || buf[0] == 'r')) {
        uint8_t addr = hex2byte(buf + 1);
        uint8_t val = ADS1292R_ReadReg(addr);
        char resp[16];
        int n = sprintf(resp, "R%02X=%02X\r\n", addr, val);
        CDC_Transmit_FS((uint8_t *)resp, n);
        return;
    }

    /* 写寄存器: "Wxx yy\n" (xx=地址, yy=数据, 都是2位十六进制) */
    if (len >= 6 && (buf[0] == 'W' || buf[0] == 'w')) {
        uint8_t addr = hex2byte(buf + 1);
        uint8_t data = hex2byte(buf + 4);
        ADS1292R_WriteReg(addr, data);
        uint8_t readback = ADS1292R_ReadReg(addr);
        char resp[24];
        int n = sprintf(resp, "W%02X=%02X rb=%02X\r\n", addr, data, readback);
        CDC_Transmit_FS((uint8_t *)resp, n);
        return;
    }

    /* Default: echo back */
    CDC_Transmit_FS(buf, len);
}

static int8_t CDC_Init_FS(void) {
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return 0;
}

static int8_t CDC_DeInit_FS(void) {
    return 0;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
    (void)length;
    switch (cmd) {
        case CDC_SET_LINE_CODING:
            memcpy(&LineCoding, pbuf, sizeof(LineCoding));
            break;
        case CDC_GET_LINE_CODING:
            memcpy(pbuf, &LineCoding, sizeof(LineCoding));
            break;
        default:
            break;
    }
    return 0;
}

static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len) {
    CDC_ReceiveCallback(pbuf, *Len);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return 0;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len) {
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc->TxState != 0) {
        return USBD_BUSY;
    }
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
