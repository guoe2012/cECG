/**
 * 数据导出 — 通过 USB CDC 输出 ECG 帧
 */
#ifndef __UART_DUMP_H
#define __UART_DUMP_H

#include "main.h"

/* === 开关: 定义此宏启用数据流输出 === */
#define DEBUG_UART_STREAM

#ifdef DEBUG_UART_STREAM
void ECG_UART_Init(void);
void ECG_UART_DumpSample(int32_t raw_ch2, int32_t flt_ch2);
#else
#define ECG_UART_Init()           ((void)0)
#define ECG_UART_DumpSample(r,f)  ((void)0)
#endif

#endif /* __UART_DUMP_H */
