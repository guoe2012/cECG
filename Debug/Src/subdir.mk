################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/ads1292r_test.c \
../Src/ads_connect_test.c \
../Src/ecg_acquisition.c \
../Src/ili9488.c \
../Src/main.c \
../Src/stm32g4xx_hal_msp.c \
../Src/stm32g4xx_it.c \
../Src/system_stm32g4xx.c 

OBJS += \
./Src/ads1292r_test.o \
./Src/ads_connect_test.o \
./Src/ecg_acquisition.o \
./Src/ili9488.o \
./Src/main.o \
./Src/stm32g4xx_hal_msp.o \
./Src/stm32g4xx_it.o \
./Src/system_stm32g4xx.o 

C_DEPS += \
./Src/ads1292r_test.d \
./Src/ads_connect_test.d \
./Src/ecg_acquisition.d \
./Src/ili9488.d \
./Src/main.d \
./Src/stm32g4xx_hal_msp.d \
./Src/stm32g4xx_it.d \
./Src/system_stm32g4xx.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32G491xx -c -IC:/Users/guoe2/Documents/cECG/Inc -IC:/Users/guoe2/Documents/cECG/Drivers/STM32G4xx_HAL_Driver/Inc -IC:/Users/guoe2/Documents/cECG/Drivers/CMSIS/Include -IC:/Users/guoe2/Documents/cECG/Drivers/CMSIS/Device/ST/STM32G4xx/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/ads1292r_test.cyclo ./Src/ads1292r_test.d ./Src/ads1292r_test.o ./Src/ads1292r_test.su ./Src/ads_connect_test.cyclo ./Src/ads_connect_test.d ./Src/ads_connect_test.o ./Src/ads_connect_test.su ./Src/ecg_acquisition.cyclo ./Src/ecg_acquisition.d ./Src/ecg_acquisition.o ./Src/ecg_acquisition.su ./Src/ili9488.cyclo ./Src/ili9488.d ./Src/ili9488.o ./Src/ili9488.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/stm32g4xx_hal_msp.cyclo ./Src/stm32g4xx_hal_msp.d ./Src/stm32g4xx_hal_msp.o ./Src/stm32g4xx_hal_msp.su ./Src/stm32g4xx_it.cyclo ./Src/stm32g4xx_it.d ./Src/stm32g4xx_it.o ./Src/stm32g4xx_it.su ./Src/system_stm32g4xx.cyclo ./Src/system_stm32g4xx.d ./Src/system_stm32g4xx.o ./Src/system_stm32g4xx.su

.PHONY: clean-Src

