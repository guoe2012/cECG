#ifndef __DFU_MANAGER_H
#define __DFU_MANAGER_H

#include <stdint.h>
#include "stm32g4xx_hal.h"

/* Magic value stored in RAM to indicate DFU mode */
#define DFU_MAGIC_VALUE         0xDF00B000

/* DFU flag as a noinit variable (survives soft reset, NOT cleared by startup code) */
extern volatile uint32_t dfu_flag_ram;
#define DFU_FLAG_PTR            (&dfu_flag_ram)

/* Function prototypes */
void DFU_Init(void);
uint8_t DFU_IsRequested(void);
void DFU_RequestEnter(void);
void DFU_ClearRequest(void);
void DFU_JumpToBootloader(void);

/* CDC command handler for DFU */
void DFU_HandleCommand(uint8_t *buf, uint32_t len);

#endif /* __DFU_MANAGER_H */
