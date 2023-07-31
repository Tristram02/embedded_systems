################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../example/src/cr_startup_lpc175x_6x.c \
../example/src/httpd.c \
../example/src/lwip_fs.c \
../example/src/sysinit.c \
../example/src/webserver.c 

OBJS += \
./example/src/cr_startup_lpc175x_6x.o \
./example/src/httpd.o \
./example/src/lwip_fs.o \
./example/src/sysinit.o \
./example/src/webserver.o 

C_DEPS += \
./example/src/cr_startup_lpc175x_6x.d \
./example/src/httpd.d \
./example/src/lwip_fs.d \
./example/src/sysinit.d \
./example/src/webserver.d 


# Each subdirectory must supply rules for building sources it contributes
example/src/%.o: ../example/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\student\Desktop\embedded_systems-main\Licznik\lwip" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_EaBaseBoard\inc" -I"C:\Users\student\Desktop\embedded_systems-main\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


