# =============================================================================
# mmWave RF Test Firmware - Build System
# Target: IWR6843 (ARM Cortex-R5F)
# =============================================================================

# Toolchain
CROSS_COMPILE ?= arm-none-eabi-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE    = $(CROSS_COMPILE)size

# Project
PROJECT  = mmwave_rf_test
VERSION  = 1.0.0

# Directories
SRC_DIR    = src
INC_DIR    = include
BUILD_DIR  = build
OBJ_DIR    = $(BUILD_DIR)/obj
BIN_DIR    = $(BUILD_DIR)/bin
TEST_DIR   = tests
LINKER_DIR = linker

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/rf_tests/tx_power_test.c \
       $(SRC_DIR)/rf_tests/rx_sensitivity_test.c \
       $(SRC_DIR)/rf_tests/phase_noise_test.c \
       $(SRC_DIR)/rf_tests/freq_accuracy_test.c \
       $(SRC_DIR)/rf_tests/noise_figure_test.c \
       $(SRC_DIR)/rf_tests/tx_evm_test.c \
       $(SRC_DIR)/calibration/tx_power_flatness_cal.c \
       $(SRC_DIR)/calibration/rx_gain_phase_cal.c \
       $(SRC_DIR)/calibration/lo_leakage_nulling.c \
       $(SRC_DIR)/calibration/inter_channel_cal.c \
       $(SRC_DIR)/drivers/synth_driver.c \
       $(SRC_DIR)/drivers/pga_driver.c \
       $(SRC_DIR)/drivers/if_filter_driver.c \
       $(SRC_DIR)/drivers/adc_driver.c \
       $(SRC_DIR)/drivers/dac_driver.c \
       $(SRC_DIR)/drivers/tx_frontend.c \
       $(SRC_DIR)/drivers/rx_frontend.c \
       $(SRC_DIR)/rom/rom_boot_hooks.c \
       $(SRC_DIR)/rom/factory_trim.c \
       $(SRC_DIR)/data/capture_engine.c \
       $(SRC_DIR)/data/dma_manager.c \
       $(SRC_DIR)/protocol/uart_cmd_handler.c \
       $(SRC_DIR)/protocol/test_protocol.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

# Linker script
LDSCRIPT = $(LINKER_DIR)/iwr6843_r5f.lds

# Compiler flags
CFLAGS  = -mcpu=cortex-r5 -mthumb -mfloat-abi=hard -mfpu=vfpv3-d16
CFLAGS += -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I$(INC_DIR) -I$(SRC_DIR)
CFLAGS += -DDEVICE_IWR6843 -DPLATFORM_R5F
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += -MMD -MP

# Linker flags
LDFLAGS  = -T$(LDSCRIPT)
LDFLAGS += --gc-sections
LDFLAGS += -Map=$(BIN_DIR)/$(PROJECT).map

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean flash test lint size help

all: $(BIN_DIR)/$(PROJECT).elf $(BIN_DIR)/$(PROJECT).bin
	@echo "Build complete: $(PROJECT) v$(VERSION)"
	@$(SIZE) $(BIN_DIR)/$(PROJECT).elf

$(BIN_DIR)/$(PROJECT).elf: $(OBJS) $(LDSCRIPT)
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc -lm
	@echo "Linked: $@"

$(BIN_DIR)/$(PROJECT).bin: $(BIN_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@
	@echo "Binary: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "Compiled: $<"

# Flash via OpenOCD/JTAG
flash: $(BIN_DIR)/$(PROJECT).bin
	openocd -f interface/xds110.cfg \
	        -f target/ti_iwr6843.cfg \
	        -c "program $(BIN_DIR)/$(PROJECT).bin 0x00000000 verify reset exit"

# Unit tests (host build with gcc)
TEST_SRCS = $(wildcard $(TEST_DIR)/unit/*.c)
TEST_BIN  = $(BUILD_DIR)/test_runner

test:
	@mkdir -p $(BUILD_DIR)
	gcc -DUNIT_TEST -DHOST_BUILD \
	    -I$(INC_DIR) -I$(SRC_DIR) \
	    -o $(TEST_BIN) $(TEST_SRCS) \
	    $(filter-out $(OBJ_DIR)/main.o, $(SRCS)) \
	    -lm
	@echo "Running unit tests..."
	./$(TEST_BIN)

# Static analysis
lint:
	@echo "Running cppcheck..."
	cppcheck --enable=all --suppress=missingInclude \
	         --inline-suppr -I$(INC_DIR) $(SRC_DIR)/
	@echo "Running clang-tidy..."
	clang-tidy $(SRCS) -- $(CFLAGS)

# Print binary size breakdown
size: $(BIN_DIR)/$(PROJECT).elf
	$(SIZE) -A $<

# Disassembly listing
disasm: $(BIN_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d -S $< > $(BIN_DIR)/$(PROJECT).lst

clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

help:
	@echo "mmWave RF Test Firmware Build System"
	@echo "  make all     - Build firmware ELF and BIN"
	@echo "  make flash   - Flash to target via JTAG"
	@echo "  make test    - Run host-side unit tests"
	@echo "  make lint    - Run static analysis"
	@echo "  make size    - Print size breakdown"
	@echo "  make disasm  - Generate disassembly listing"
	@echo "  make clean   - Remove build artifacts"

-include $(DEPS)
