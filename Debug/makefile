################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include freemodbus-v1.5.0/port/subdir.mk
-include freemodbus-v1.5.0/modbus/tcp/subdir.mk
-include freemodbus-v1.5.0/modbus/rtu/subdir.mk
-include freemodbus-v1.5.0/modbus/functions/subdir.mk
-include freemodbus-v1.5.0/modbus/ascii/subdir.mk
-include freemodbus-v1.5.0/modbus/subdir.mk
-include Startup/subdir.mk
-include Libraries/XMCLib/src/subdir.mk
-include Libraries/Newlib/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
FlapFlap.hex \

SECONDARY_LIST += \
FlapFlap.lst \

SECONDARY_SIZE += \
FlapFlap.siz \


LIST_FILES = $(addsuffix .o.lst, $(basename $(OBJS))) 
MAP_FILE = "FlapFlap.map"

TOOLCHAIN_ROOT=C:/DAVEv4-64Bit/DAVE-4.3.2/eclipse/ARM-GCC-49
PROJECT_LOC=C:/Projekte/Infineon_Dave/eHaJo/FlapFlap

# All Target
all: FlapFlap.elf secondary-outputs

# Tool invocations
FlapFlap.elf: $(OBJS) ../linker_script.ld $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM-GCC C Linker'
	$(file >objects.rsp,$(OBJS))
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -T"../linker_script.ld" -nostartfiles -Xlinker --gc-sections -specs=nano.specs -specs=nosys.specs -Wl,-Map,$(MAP_FILE) -mcpu=cortex-m0 -mthumb -g -gdwarf-2 -o "FlapFlap.elf" "@objects.rsp" $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo.
FlapFlap.hex: FlapFlap.elf
	@echo 'Invoking: ARM-GCC Create Flash Image'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-objcopy" -O ihex "FlapFlap.elf" "FlapFlap.hex"
	@echo 'Finished building: $@'
	@echo.
FlapFlap.lst: FlapFlap.elf
	@echo 'Invoking: ARM-GCC Create Listing'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-objdump" -h -S "FlapFlap.elf" > "FlapFlap.lst"
	@echo 'Finished building: $@'
	@echo.
FlapFlap.siz: FlapFlap.elf
	@echo 'Invoking: ARM-GCC Print Size'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-size" --format=berkeley "FlapFlap.elf"
	@echo 'Finished building: $@'
	@echo.
# Other Targets
clean:
	-$(RM) $(OBJS)$(SECONDARY_FLASH)$(SECONDARY_LIST)$(SECONDARY_SIZE)$(ARM_GCC_FOR_XMC_LINKER_OUTPUT_TYPE_OUTPUTS)$(S_DEPS)$(S_UPPER_DEPS)$(C_DEPS) FlapFlap.elf $(LIST_FILES) $(MAP_FILE) objects.rsp
	-@echo.
secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
