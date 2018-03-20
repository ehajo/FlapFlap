################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../freemodbus-v1.5.0/modbus/functions/mbfunccoils.c \
../freemodbus-v1.5.0/modbus/functions/mbfuncdiag.c \
../freemodbus-v1.5.0/modbus/functions/mbfuncdisc.c \
../freemodbus-v1.5.0/modbus/functions/mbfuncholding.c \
../freemodbus-v1.5.0/modbus/functions/mbfuncinput.c \
../freemodbus-v1.5.0/modbus/functions/mbfuncother.c \
../freemodbus-v1.5.0/modbus/functions/mbutils.c 

OBJS += \
./freemodbus-v1.5.0/modbus/functions/mbfunccoils.o \
./freemodbus-v1.5.0/modbus/functions/mbfuncdiag.o \
./freemodbus-v1.5.0/modbus/functions/mbfuncdisc.o \
./freemodbus-v1.5.0/modbus/functions/mbfuncholding.o \
./freemodbus-v1.5.0/modbus/functions/mbfuncinput.o \
./freemodbus-v1.5.0/modbus/functions/mbfuncother.o \
./freemodbus-v1.5.0/modbus/functions/mbutils.o 

C_DEPS += \
./freemodbus-v1.5.0/modbus/functions/mbfunccoils.d \
./freemodbus-v1.5.0/modbus/functions/mbfuncdiag.d \
./freemodbus-v1.5.0/modbus/functions/mbfuncdisc.d \
./freemodbus-v1.5.0/modbus/functions/mbfuncholding.d \
./freemodbus-v1.5.0/modbus/functions/mbfuncinput.d \
./freemodbus-v1.5.0/modbus/functions/mbfuncother.d \
./freemodbus-v1.5.0/modbus/functions/mbutils.d 


# Each subdirectory must supply rules for building sources it contributes
freemodbus-v1.5.0/modbus/functions/%.o: ../freemodbus-v1.5.0/modbus/functions/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC1100_T038x0064 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/ascii" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/rtu" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/port" -I"$(PROJECT_LOC)/freemodbus-v1.5.0/modbus/include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC1100_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Libraries" -Og -ffunction-sections -fdata-sections -Wall -std=gnu99 -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m0 -mthumb -g -gdwarf-2 -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

