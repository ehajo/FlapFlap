################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../freemodbus-v1.5.0/port/portevent.c \
../freemodbus-v1.5.0/port/portserial.c \
../freemodbus-v1.5.0/port/porttcp.c \
../freemodbus-v1.5.0/port/porttimer.c 

OBJS += \
./freemodbus-v1.5.0/port/portevent.o \
./freemodbus-v1.5.0/port/portserial.o \
./freemodbus-v1.5.0/port/porttcp.o \
./freemodbus-v1.5.0/port/porttimer.o 

C_DEPS += \
./freemodbus-v1.5.0/port/portevent.d \
./freemodbus-v1.5.0/port/portserial.d \
./freemodbus-v1.5.0/port/porttcp.d \
./freemodbus-v1.5.0/port/porttimer.d 


# Each subdirectory must supply rules for building sources it contributes
freemodbus-v1.5.0/port/%.o: ../freemodbus-v1.5.0/port/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC1100_T038x0064 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/ascii" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/rtu" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/port" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC1100_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Libraries" -Og -ffunction-sections -fdata-sections -Wall -std=gnu99 -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m0 -mthumb -g -gdwarf-2 -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

