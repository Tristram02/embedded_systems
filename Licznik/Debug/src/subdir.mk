################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cr_startup_lpc17.c \
../src/eeprom.c \
../src/enet.c \
../src/ethernet.c \
../src/ff.c \
../src/httpd.c \
../src/lpc17xx_spi.c \
../src/lwip_fs.c \
../src/main.c \
../src/mmc.c 

OBJS += \
./src/cr_startup_lpc17.o \
./src/eeprom.o \
./src/enet.o \
./src/ethernet.o \
./src/ff.o \
./src/httpd.o \
./src/lpc17xx_spi.o \
./src/lwip_fs.o \
./src/main.o \
./src/mmc.o 

C_DEPS += \
./src/cr_startup_lpc17.d \
./src/eeprom.d \
./src/enet.d \
./src/ethernet.d \
./src/ff.d \
./src/httpd.d \
./src/lpc17xx_spi.d \
./src/lwip_fs.d \
./src/main.d \
./src/mmc.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\student\Documents\Licznik\lpc_board_nxp_lpcxpresso_1769\inc" -I"C:\Users\student\Documents\Licznik\Licznik\inc" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc\ipv4" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_EaBaseBoard\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


