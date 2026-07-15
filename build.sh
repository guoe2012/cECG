#!/bin/bash
# cECG 固件编译脚本 (Linux / arm-none-eabi-gcc)
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build_linux"
CROSS=arm-none-eabi

CPU_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
CFLAGS="$CPU_FLAGS -DSTM32G491xx -DUSE_HAL_DRIVER -O0 -g3 -Wall -ffunction-sections -fdata-sections"

INCLUDES=(
  "-I$ROOT/Inc"
  "-I$ROOT/Drivers/STM32G4xx_HAL_Driver/Inc"
  "-I$ROOT/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"
  "-I$ROOT/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
  "-I$ROOT/Drivers/CMSIS/Include"
  "-I$ROOT/Drivers/CMSIS/Core/Include"
  # USB Library includes
  "-I$ROOT/STM32_USB_Device_Library/Core/Inc"
  "-I$ROOT/STM32_USB_Device_Library/Class/CDC/Inc"
  "-I$ROOT/STM32_USB_Device_Library/Class/DFU/Inc"
  "-I$ROOT/USB_Device/App"
  "-I$ROOT/USB_Device/Target"
)

LD_SCRIPT="$ROOT/STM32G491RETx_FLASH.ld"
STARTUP="$ROOT/startup_stm32g491xx.s"

# 收集 HAL 驱动源文件（只编译实际需要的）
HAL_SRC="$ROOT/Drivers/STM32G4xx_HAL_Driver/Src"
HAL_FILES=(
  stm32g4xx_hal.c
  stm32g4xx_hal_cortex.c
  stm32g4xx_hal_gpio.c
  stm32g4xx_hal_rcc.c
  stm32g4xx_hal_rcc_ex.c
  stm32g4xx_hal_spi.c
  stm32g4xx_hal_spi_ex.c
  stm32g4xx_hal_dma.c
  stm32g4xx_hal_dma_ex.c
  stm32g4xx_hal_flash.c
  stm32g4xx_hal_flash_ex.c
  stm32g4xx_hal_flash_ramfunc.c
  stm32g4xx_hal_pwr.c
  stm32g4xx_hal_pwr_ex.c
  stm32g4xx_hal_exti.c
  stm32g4xx_hal_tim.c
  stm32g4xx_hal_tim_ex.c
  stm32g4xx_hal_uart.c
  stm32g4xx_hal_uart_ex.c
  # USB HAL driver
  stm32g4xx_hal_pcd.c
  stm32g4xx_hal_pcd_ex.c
  stm32g4xx_ll_usb.c
)

# USB Device Library source files
USB_CORE_SRC="$ROOT/STM32_USB_Device_Library/Core/Src"
USB_CORE_FILES=(
  usbd_core.c
  usbd_ctlreq.c
  usbd_ioreq.c
)

USB_CDC_SRC="$ROOT/STM32_USB_Device_Library/Class/CDC/Src"
USB_CDC_FILES=(
  usbd_cdc.c
)

# USB DFU Class Library source files
USB_DFU_SRC="$ROOT/STM32_USB_Device_Library/Class/DFU/Src"
USB_DFU_FILES=(
  usbd_dfu.c
)

# 用户源文件
USER_FILES=(
  "$ROOT/Src/main.c"
  "$ROOT/Src/stm32g4xx_hal_msp.c"
  "$ROOT/Src/stm32g4xx_it.c"
  "$ROOT/Src/system_stm32g4xx.c"
  "$ROOT/Src/ecg_acquisition.c"
  "$ROOT/Src/ili9488.c"
  "$ROOT/Src/ads1292r_test.c"
  "$ROOT/Src/ads_connect_test.c"
  "$ROOT/Src/uart_dump.c"
  # DFU support files
  "$ROOT/Src/dfu_manager.c"
  "$ROOT/Src/usb_dfu_media.c"
)

# USB Device application files
USB_APP_FILES=(
  "$ROOT/USB_Device/App/usbd_cdc_if.c"
  "$ROOT/USB_Device/App/usbd_desc.c"
  "$ROOT/USB_Device/App/usb_device.c"
  "$ROOT/USB_Device/Target/usbd_conf.c"
)

mkdir -p "$BUILD/hal" "$BUILD/usr" "$BUILD/usb_core" "$BUILD/usb_cdc" "$BUILD/usb_dfu" "$BUILD/usb_app"

echo "=== 编译 HAL 驱动 ==="
OBJ_LIST=()
for f in "${HAL_FILES[@]}"; do
  src="$HAL_SRC/$f"
  obj="$BUILD/hal/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done

echo ""
echo "=== 编译 USB 核心库 ==="
for f in "${USB_CORE_FILES[@]}"; do
  src="$USB_CORE_SRC/$f"
  obj="$BUILD/usb_core/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done

echo ""
echo "=== 编译 USB CDC 类 ==="
for f in "${USB_CDC_FILES[@]}"; do
  src="$USB_CDC_SRC/$f"
  obj="$BUILD/usb_cdc/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done

echo ""
echo "=== 编译 USB DFU 类 ==="
for f in "${USB_DFU_FILES[@]}"; do
  src="$USB_DFU_SRC/$f"
  obj="$BUILD/usb_dfu/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done
for src in "${USB_APP_FILES[@]}"; do
  f=$(basename "$src")
  obj="$BUILD/usb_app/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done

echo ""
echo "=== 编译用户代码 ==="
for src in "${USER_FILES[@]}"; do
  f=$(basename "$src")
  obj="$BUILD/usr/${f%.c}.o"
  if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
    echo "  CC $f"
    $CROSS-gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
  else
    echo "  -- $f (up-to-date)"
  fi
  OBJ_LIST+=("$obj")
done

echo ""
echo "=== 汇编启动文件 ==="
STARTUP_OBJ="$BUILD/startup_stm32g491xx.o"
if [ "$STARTUP" -nt "$STARTUP_OBJ" ] || [ ! -f "$STARTUP_OBJ" ]; then
  echo "  AS startup_stm32g491xx.s"
  $CROSS-gcc $CPU_FLAGS -c "$STARTUP" -o "$STARTUP_OBJ"
fi
OBJ_LIST+=("$STARTUP_OBJ")

echo ""
echo "=== 链接 ==="
ELF="$BUILD/cECG.elf"
$CROSS-gcc $CPU_FLAGS \
  -T"$LD_SCRIPT" \
  --specs=nosys.specs --specs=nano.specs \
  -Wl,-Map="$BUILD/cECG.map" -Wl,--gc-sections \
  -Wl,--start-group -lc -lm -Wl,--end-group \
  "${OBJ_LIST[@]}" \
  -o "$ELF"

echo ""
echo "=== 固件大小 ==="
$CROSS-size "$ELF"

echo ""
echo "=== 生成 BIN 文件 ==="
BIN="$BUILD/cECG.bin"
$CROSS-objcopy -O binary "$ELF" "$BIN"
echo "  BIN: $BIN ($(stat -c%s "$BIN") bytes)"

echo ""
echo "✅ 编译完成: $ELF"
echo "   烧录命令: dfu-util -a 0 -s 0x08000000:leave -D $BIN"
