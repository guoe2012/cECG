/**
 * 数据导出模块
 * 通过 USB CDC 输出原始 ECG 数据（二进制帧格式）
 * 用于信号质量分析 — 判断心跳信号是否存在
 *
 * 数据格式 (二进制, 小端):
 *   帧头: 0xA5 0x5A (2字节)
 *   seq:  序号 (4字节 uint32_t)
 *   raw:  原始 CH2 ADC 值 (4字节 int32_t)
 *   flt:  滤波后 CH2 值 (4字节 int32_t)
 *   = 每帧 14 字节, @500SPS = 7000 字节/秒
 *
 * 上位机采集: cat /dev/ttyACM0 | python3 parse_frames.py
 */

#include "main.h"
#include "uart_dump.h"
#include "usbd_cdc_if.h"
#include <string.h>

#ifdef DEBUG_UART_STREAM

static uint32_t dump_seq = 0;

void ECG_UART_Init(void) {
    dump_seq = 0;

    /* 发送同步头: 32字节 0x00 + 帧头 0xA5 0x5A, 用于上位机自动找帧 */
    uint8_t sync[34];
    memset(sync, 0, 32);
    sync[32] = 0xA5;
    sync[33] = 0x5A;

    /* CDC 发送可能 BUSY，重试几次 */
    for (int retry = 0; retry < 10; retry++) {
        if (CDC_Transmit_FS(sync, 34) == USBD_OK) break;
        HAL_Delay(1);
    }
}

void ECG_UART_DumpSample(int32_t raw_ch2, int32_t flt_ch2) {
    /* 帧格式: [0xA5][0x5A][seq:4][raw:4][flt:4] = 14 bytes */
    uint8_t frame[14];
    frame[0] = 0xA5;
    frame[1] = 0x5A;
    frame[2] = (uint8_t)(dump_seq);
    frame[3] = (uint8_t)(dump_seq >> 8);
    frame[4] = (uint8_t)(dump_seq >> 16);
    frame[5] = (uint8_t)(dump_seq >> 24);
    frame[6]  = (uint8_t)(raw_ch2);
    frame[7]  = (uint8_t)(raw_ch2 >> 8);
    frame[8]  = (uint8_t)(raw_ch2 >> 16);
    frame[9]  = (uint8_t)(raw_ch2 >> 24);
    frame[10] = (uint8_t)(flt_ch2);
    frame[11] = (uint8_t)(flt_ch2 >> 8);
    frame[12] = (uint8_t)(flt_ch2 >> 16);
    frame[13] = (uint8_t)(flt_ch2 >> 24);

    /* 非阻塞发送，丢帧比卡住采样好 */
    CDC_Transmit_FS(frame, 14);
    dump_seq++;
}

#endif /* DEBUG_UART_STREAM */
