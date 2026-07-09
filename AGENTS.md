# cECG — STM32G491 ECG Acquisition Firmware

## Project Overview
Embedded C firmware for a clinical-grade ECG (electrocardiogram) acquisition device based on the STM32G491RET6 microcontroller.

## Hardware
- **MCU**: STM32G491RET6 (ARM Cortex-M4, FPv5, 170 MHz)
- **AFE**: ADS1292R (24-bit dual-channel delta-sigma ADC for ECG)
- **Display**: ILI9488 (480x320 TFT LCD, SPI interface)
- **Connectivity**: On-device test modes for AFE validation

## Project Structure
- `Src/` — Source files
  - `main.c` — Entry point, initialization, main loop
  - `ecg_acquisition.c` — ECG data acquisition logic
  - `ads1292r_test.c` — ADS1292R AFE test routines
  - `ads_connect_test.c` — Connection test for ADS1292R
  - `ili9488.c` — Display driver
  - `stm32g4xx_hal_msp.c` — HAL MSP initialization
  - `stm32g4xx_it.c` — Interrupt handlers
  - `system_stm32g4xx.c` — System clock config
- `Inc/` — Header files
- `Drivers/` — STM32 HAL and CMSIS drivers
- `build_linux/` — Build output directory
- `Debug/` — Debug build artifacts (IDE-generated)

## Build
```bash
./build.sh
```
Output: `build_linux/cECG.elf`

Toolchain: `arm-none-eabi-gcc` with flags `-mcpu=cortex-m4 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard`

## Debug/Flash
OpenOCD with ST-Link:
```
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c 'program build_linux/cECG.elf verify reset exit'
```

## Key Peripherals
- **SPI1**: ILI9488 display
- **SPI2**: ADS1292R AFE
- **TIM2**: ADC timing/trigger
- **DMA**: Data transfer for ADC
