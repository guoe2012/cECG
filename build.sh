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
)

mkdir -p "$BUILD/hal" "$BUILD/usr"

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
echo "✅ 编译完成: $ELF"
