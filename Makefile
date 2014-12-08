################################################################################
# Makefile to build the improved T-962 firmware without LPCXpresso
#
# Makes a 'build' directory in the root of the project.
################################################################################
BASE_NAME := T-962-controller

SRC_DIR := ./src/
BUILD_DIR := ./build/
TARGET := $(BUILD_DIR)$(BASE_NAME).axf

vpath %.c $(SRC_DIR)
vpath %.o $(BUILD_DIR)
vpath %.d $(BUILD_DIR)

CC := arm-none-eabi-gcc
RM := rm -rf

# Source files
C_SRCS += $(wildcard src/*.c)

S_SRCS += cr_startup_lpc21.s import.s

OBJS := $(BUILD_DIR)PID_v1.o \
$(BUILD_DIR)adc.o \
$(BUILD_DIR)buzzer.o \
$(BUILD_DIR)cr_startup_lpc21.o \
$(BUILD_DIR)crp.o \
$(BUILD_DIR)eeprom.o \
$(BUILD_DIR)i2c.o \
$(BUILD_DIR)import.o \
$(BUILD_DIR)io.o \
$(BUILD_DIR)keypad.o \
$(BUILD_DIR)lcd.o \
$(BUILD_DIR)main.o \
$(BUILD_DIR)onewire.o \
$(BUILD_DIR)reflow.o \
$(BUILD_DIR)rtc.o \
$(BUILD_DIR)sched.o \
$(BUILD_DIR)serial.o

C_DEPS := $(wildcard *.d)

all: axf

create_build_dir:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: $(SRC_DIR)%.c create_build_dir
	$(CC) -std=gnu99 -DNDEBUG -D__NEWLIB__ -Os -g -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -flto -ffat-lto-objects -mcpu=arm7tdmi -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

$(BUILD_DIR)%.o: $(SRC_DIR)%.s
	@echo 'Building file: $<'
	$(CC) -c -x assembler-with-cpp -I $(BUILD_DIR) -DNDEBUG -D__NEWLIB__ -mcpu=arm7tdmi -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	$(CC) -nostdlib -Xlinker -Map="$(BUILD_DIR)$(BASE_NAME).map" -Xlinker --gc-sections -mcpu=arm7tdmi -T "$(BASE_NAME).ld" -o "$(TARGET)" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build


clean:
	-$(RM) $(BUILD_DIR)
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size "$(TARGET)";
	-arm-none-eabi-objcopy -v -O ihex "$(TARGET)" "$(BUILD_DIR)$(BASE_NAME).hex"
	-@echo ' '

.PHONY: clean dependents
.SECONDARY: post-build

-include ../makefile.targets
