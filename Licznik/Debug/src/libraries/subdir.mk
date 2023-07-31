################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/libraries/acc.c \
../src/libraries/board.c \
../src/libraries/clock_17xx_40xx.c \
../src/libraries/cr_startup_lpc17.c \
../src/libraries/cr_startup_lpc175x_6x.c \
../src/libraries/eeprom.c \
../src/libraries/enet_17xx_40xx.c \
../src/libraries/font5x7.c \
../src/libraries/light.c \
../src/libraries/lpc17xx_adc.c \
../src/libraries/lpc17xx_can.c \
../src/libraries/lpc17xx_clkpwr.c \
../src/libraries/lpc17xx_dac.c \
../src/libraries/lpc17xx_emac.c \
../src/libraries/lpc17xx_exti.c \
../src/libraries/lpc17xx_gpdma.c \
../src/libraries/lpc17xx_gpio.c \
../src/libraries/lpc17xx_i2c.c \
../src/libraries/lpc17xx_i2s.c \
../src/libraries/lpc17xx_libcfg_default.c \
../src/libraries/lpc17xx_mcpwm.c \
../src/libraries/lpc17xx_nvic.c \
../src/libraries/lpc17xx_pinsel.c \
../src/libraries/lpc17xx_pwm.c \
../src/libraries/lpc17xx_qei.c \
../src/libraries/lpc17xx_rit.c \
../src/libraries/lpc17xx_rtc.c \
../src/libraries/lpc17xx_spi.c \
../src/libraries/lpc17xx_ssp.c \
../src/libraries/lpc17xx_systick.c \
../src/libraries/lpc17xx_timer.c \
../src/libraries/lpc17xx_uart.c \
../src/libraries/lpc17xx_wdt.c \
../src/libraries/lpc_phy_smsc87x0.c \
../src/libraries/oled.c \
../src/libraries/pca9532.c \
../src/libraries/rgb.c \
../src/libraries/system_LPC17xx.c \
../src/libraries/temp.c \
../src/libraries/uart_17xx_40xx.c 

OBJS += \
./src/libraries/acc.o \
./src/libraries/board.o \
./src/libraries/clock_17xx_40xx.o \
./src/libraries/cr_startup_lpc17.o \
./src/libraries/cr_startup_lpc175x_6x.o \
./src/libraries/eeprom.o \
./src/libraries/enet_17xx_40xx.o \
./src/libraries/font5x7.o \
./src/libraries/light.o \
./src/libraries/lpc17xx_adc.o \
./src/libraries/lpc17xx_can.o \
./src/libraries/lpc17xx_clkpwr.o \
./src/libraries/lpc17xx_dac.o \
./src/libraries/lpc17xx_emac.o \
./src/libraries/lpc17xx_exti.o \
./src/libraries/lpc17xx_gpdma.o \
./src/libraries/lpc17xx_gpio.o \
./src/libraries/lpc17xx_i2c.o \
./src/libraries/lpc17xx_i2s.o \
./src/libraries/lpc17xx_libcfg_default.o \
./src/libraries/lpc17xx_mcpwm.o \
./src/libraries/lpc17xx_nvic.o \
./src/libraries/lpc17xx_pinsel.o \
./src/libraries/lpc17xx_pwm.o \
./src/libraries/lpc17xx_qei.o \
./src/libraries/lpc17xx_rit.o \
./src/libraries/lpc17xx_rtc.o \
./src/libraries/lpc17xx_spi.o \
./src/libraries/lpc17xx_ssp.o \
./src/libraries/lpc17xx_systick.o \
./src/libraries/lpc17xx_timer.o \
./src/libraries/lpc17xx_uart.o \
./src/libraries/lpc17xx_wdt.o \
./src/libraries/lpc_phy_smsc87x0.o \
./src/libraries/oled.o \
./src/libraries/pca9532.o \
./src/libraries/rgb.o \
./src/libraries/system_LPC17xx.o \
./src/libraries/temp.o \
./src/libraries/uart_17xx_40xx.o 

C_DEPS += \
./src/libraries/acc.d \
./src/libraries/board.d \
./src/libraries/clock_17xx_40xx.d \
./src/libraries/cr_startup_lpc17.d \
./src/libraries/cr_startup_lpc175x_6x.d \
./src/libraries/eeprom.d \
./src/libraries/enet_17xx_40xx.d \
./src/libraries/font5x7.d \
./src/libraries/light.d \
./src/libraries/lpc17xx_adc.d \
./src/libraries/lpc17xx_can.d \
./src/libraries/lpc17xx_clkpwr.d \
./src/libraries/lpc17xx_dac.d \
./src/libraries/lpc17xx_emac.d \
./src/libraries/lpc17xx_exti.d \
./src/libraries/lpc17xx_gpdma.d \
./src/libraries/lpc17xx_gpio.d \
./src/libraries/lpc17xx_i2c.d \
./src/libraries/lpc17xx_i2s.d \
./src/libraries/lpc17xx_libcfg_default.d \
./src/libraries/lpc17xx_mcpwm.d \
./src/libraries/lpc17xx_nvic.d \
./src/libraries/lpc17xx_pinsel.d \
./src/libraries/lpc17xx_pwm.d \
./src/libraries/lpc17xx_qei.d \
./src/libraries/lpc17xx_rit.d \
./src/libraries/lpc17xx_rtc.d \
./src/libraries/lpc17xx_spi.d \
./src/libraries/lpc17xx_ssp.d \
./src/libraries/lpc17xx_systick.d \
./src/libraries/lpc17xx_timer.d \
./src/libraries/lpc17xx_uart.d \
./src/libraries/lpc17xx_wdt.d \
./src/libraries/lpc_phy_smsc87x0.d \
./src/libraries/oled.d \
./src/libraries/pca9532.d \
./src/libraries/rgb.d \
./src/libraries/system_LPC17xx.d \
./src/libraries/temp.d \
./src/libraries/uart_17xx_40xx.d 


# Each subdirectory must supply rules for building sources it contributes
src/libraries/%.o: ../src/libraries/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__CODE_RED -D__NEWLIB__ -I"C:\Users\student\Documents\licznik\Licznik\inc\libraries" -I"C:\Users\student\Documents\licznik\Licznik\inc" -I"C:\Users\student\Documents\licznik\Licznik\lwip\inc" -I"C:\Users\student\Documents\licznik\Licznik\lwip\inc\ipv4" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


