################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lwip/src/arch/lpc17xx_40xx_emac.c \
../lwip/src/arch/lpc17xx_40xx_systick_arch.c \
../lwip/src/arch/lpc_debug.c 

OBJS += \
./lwip/src/arch/lpc17xx_40xx_emac.o \
./lwip/src/arch/lpc17xx_40xx_systick_arch.o \
./lwip/src/arch/lpc_debug.o 

C_DEPS += \
./lwip/src/arch/lpc17xx_40xx_emac.d \
./lwip/src/arch/lpc17xx_40xx_systick_arch.d \
./lwip/src/arch/lpc_debug.d 


# Each subdirectory must supply rules for building sources it contributes
lwip/src/arch/%.o: ../lwip/src/arch/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\student\Documents\Licznik\lpc_board_nxp_lpcxpresso_1769\inc" -I"C:\Users\student\Documents\Licznik\Licznik\inc" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc\ipv4" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_EaBaseBoard\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


