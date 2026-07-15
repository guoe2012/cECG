/**
 * UART 数据导出模块
 * 通过 USART2 (PA2=TX) 以 921600 波特率输出原始 ECG 数据
 * 用于信号质量分析 — 判断心跳信号是否存在
 *
 * 数据格式 (二进制, 小端):
 *   帧头: 0xA5 0x5A (2字节)
 *   seq:  序号 (4字节 uint32_t)
 *   raw:  原始 CH2 ADC 值 (4字节 int32_t)
 *   flt:  滤波后 CH2 值 (4字节 int32_t)
 *   = 每帧 14 字节, @500SPS = 7000 字节/秒 (远低于 921600bps 的 ~92KB/s 上限)
 *
 * 启用方式: 在 ecg_acquisition.h 中 #define DEBUG_UART_STREAM
 */

#include "main.h"
#include "uart_dump.h"
#include <string.h>

#ifdef DEBUG_UART_STREAM

static UART_HandleTypeDef huart2;

/* USART2 TX 引脚: PA2=TX (AF7) */
static void UART2_Init(void) {
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 921600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* 阻塞发送一帧数据 */
static uint32_t dump_seq = 0;

void ECG_UART_Init(void) {
    UART2_Init();
    dump_seq = 0;

    /* 发送同步头: 32字节 0x00 + 帧头 0xA5 0x5A, 用于上位机自动找帧 */
    uint8_t sync[34];
    memset(sync, 0, 32);
    sync[32] = 0xA5;
    sync[33] = 0x5A;
    HAL_UART_Transmit(&huart2, sync, 34, 100);
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

    HAL_UART_Transmit(&huart2, frame, 14, 10);
    dump_seq++;
}

#endif /* DEBUG_UART_STREAM */
