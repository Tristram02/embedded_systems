################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lwip/src/core/ipv4/autoip.c \
../lwip/src/core/ipv4/icmp.c \
../lwip/src/core/ipv4/igmp.c \
../lwip/src/core/ipv4/inet.c \
../lwip/src/core/ipv4/inet_chksum.c \
../lwip/src/core/ipv4/ip.c \
../lwip/src/core/ipv4/ip_addr.c \
../lwip/src/core/ipv4/ip_frag.c 

OBJS += \
./lwip/src/core/ipv4/autoip.o \
./lwip/src/core/ipv4/icmp.o \
./lwip/src/core/ipv4/igmp.o \
./lwip/src/core/ipv4/inet.o \
./lwip/src/core/ipv4/inet_chksum.o \
./lwip/src/core/ipv4/ip.o \
./lwip/src/core/ipv4/ip_addr.o \
./lwip/src/core/ipv4/ip_frag.o 

C_DEPS += \
./lwip/src/core/ipv4/autoip.d \
./lwip/src/core/ipv4/icmp.d \
./lwip/src/core/ipv4/igmp.d \
./lwip/src/core/ipv4/inet.d \
./lwip/src/core/ipv4/inet_chksum.d \
./lwip/src/core/ipv4/ip.d \
./lwip/src/core/ipv4/ip_addr.d \
./lwip/src/core/ipv4/ip_frag.d 


# Each subdirectory must supply rules for building sources it contributes
lwip/src/core/ipv4/%.o: ../lwip/src/core/ipv4/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\student\Documents\Licznik\lpc_board_nxp_lpcxpresso_1769\inc" -I"C:\Users\student\Documents\Licznik\Licznik\inc" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc\ipv4" -I"C:\Users\student\Documents\Licznik\Licznik\lwip\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_EaBaseBoard\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


