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

# Flash tool settings
FLASH_TOOL := ./lpc21isp
FLASH_TTY := /dev/ttyUSB0
FLASH_BAUD := 57600
MCU_CLOCK := 11059

COLOR_GREEN = $(shell echo "\033[0;32m")
COLOR_RED = $(shell echo "\033[0;31m")
COLOR_END = $(shell echo "\033[0m")

# Source files
C_SRCS += $(wildcard $(SRC_DIR)*.c) $(BUILD_DIR)version.c

S_SRCS += $(wildcard $(SRC_DIR)*.S)

# filter out src/version.c created by PlatformIO
TMPVAR := $(C_SRCS)
C_SRCS = $(filter-out $(SRC_DIR)version.c, $(TMPVAR))

OBJS := $(patsubst $(SRC_DIR)%.c,$(BUILD_DIR)%.o,$(C_SRCS)) $(patsubst $(SRC_DIR)%.S,$(BUILD_DIR)%.o,$(S_SRCS))

C_DEPS := $(wildcard *.d)

all: axf

$(BUILD_DIR)version.c: $(BUILD_DIR)tag
	git describe --tag --always --dirty | \
		sed 's/.*/const char* Version_GetGitVersion(void) { return "&"; }/' > $@
# Always regenerate the git version
.PHONY: $(BUILD_DIR)version.c

$(BUILD_DIR)tag:
	$(RM) $(SRC_DIR)version.c 
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_DIR)tag
	# platformio will generate a version.c in this place, which messes with the build.
	# remove it we're building from the makefile..

$(BUILD_DIR)%.o: $(SRC_DIR)%.c $(BUILD_DIR)tag
	@echo 'Building file: $<'
	$(CC) -std=gnu99 -DNDEBUG -D__NEWLIB__ -Os -g -Wall -Wunused -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -flto -ffat-lto-objects -mcpu=arm7tdmi -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $(COLOR_GREEN)$<$(COLOR_END)'
	@echo ' '

$(BUILD_DIR)%.o: $(SRC_DIR)%.S $(BUILD_DIR)tag
	@echo 'Building file: $<'
	$(CC) -c -x assembler-with-cpp -I $(BUILD_DIR) -DNDEBUG -D__NEWLIB__ -mcpu=arm7tdmi -o "$@" "$<"
	@echo 'Finished building: $(COLOR_GREEN)$<$(COLOR_END)'
	@echo ' '


axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	$(CC) -nostdlib -Xlinker -Map="$(BUILD_DIR)$(BASE_NAME).map" -Xlinker --gc-sections -flto -Os -mcpu=arm7tdmi --specs=nano.specs -u _printf_float -u _scanf_float -T "$(BASE_NAME).ld" -o "$(TARGET)" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $(COLOR_GREEN)$@$(COLOR_END)'
	@echo ' '
	$(MAKE) --no-print-directory post-build

clean:
	-$(RM) $(BUILD_DIR)
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-gcc --version
	-arm-none-eabi-size "$(TARGET)";
	-arm-none-eabi-objcopy -v -O ihex "$(TARGET)" "$(BUILD_DIR)$(BASE_NAME).hex"
	-@echo ' '

lpc21isp: $(BUILD_DIR)tag
	-@echo ''
	-@echo 'Downloading lpc21isp 1.97 source from sourceforge'
	wget http://sourceforge.net/projects/lpc21isp/files/lpc21isp/1.97/lpc21isp_197.zip/download -O $(BUILD_DIR)lpc21isp.zip
	unzip -qq -o $(BUILD_DIR)lpc21isp.zip -d $(BUILD_DIR)

	-@echo 'Making lpc21isp'
	$(MAKE) -C $(BUILD_DIR)lpc21isp_197/
	-@echo 'Copy lpc21isp binary to current directory'
	cp $(BUILD_DIR)lpc21isp_197/lpc21isp .
	-@echo ''

flash: axf lpc21isp
	@echo ''
	@echo 'Flashing $(COLOR_GREEN)$(BASE_NAME).hex$(COLOR_END) to $(COLOR_RED)$(FLASH_TTY)$(COLOR_END)'
	$(FLASH_TOOL) "$(BUILD_DIR)$(BASE_NAME).hex" $(FLASH_TTY) $(FLASH_BAUD) $(MCU_CLOCK)

.PHONY: clean dependents
.SECONDARY: post-build

-include ../makefile.targets
