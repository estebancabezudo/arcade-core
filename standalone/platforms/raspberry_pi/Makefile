# Native standalone build; invoke here or through the repository root Makefile.
MODEL ?= 4
TOOLCHAIN ?= gcc
CROSS_COMPILE ?= aarch64-linux-gnu-
PYTHON ?= python3
ROOT := $(abspath ../../..)
STANDALONE := $(ROOT)/standalone
BUILD := $(ROOT)/build/standalone/raspberry_pi/model$(MODEL)/$(TOOLCHAIN)
GENERATED := $(ROOT)/build/standalone/raspberry_pi/generated
CORE := $(ROOT)/build/standalone/native/pacman.c
ROM_DIR ?= $(ROOT)/common/roms/pacman
EXAMPLE := $(ROOT)/build/standalone/raspberry_pi/examples/pi4_usb

ifeq ($(MODEL),3)
RPI_PERIPHERAL_BASE := 0x3F000000UL
CPU := cortex-a53
else ifeq ($(MODEL),4)
RPI_PERIPHERAL_BASE := 0xFE000000UL
CPU := cortex-a72
else
$(error MODEL must be 3 or 4)
endif

ifeq ($(TOOLCHAIN),clang)
CC := clang
OBJCOPY := llvm-objcopy
TARGET_FLAGS := --target=aarch64-none-elf
LINKER_SELECT := -fuse-ld=lld
else ifeq ($(TOOLCHAIN),gcc)
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
TARGET_FLAGS :=
LINKER_SELECT :=
else
$(error TOOLCHAIN must be gcc or clang)
endif

CFLAGS := $(TARGET_FLAGS) -std=c17 -O3 -flto -ffreestanding -fno-builtin \
    -fno-stack-protector -fno-pic -fno-pie -ffunction-sections -fdata-sections \
    -mgeneral-regs-only -mcpu=$(CPU) -Wall -Wextra -Wpedantic -Werror \
    -MMD -MP -DRPI_MODEL=$(MODEL) -DRPI_PERIPHERAL_BASE=$(RPI_PERIPHERAL_BASE) \
    -I$(STANDALONE)/include -Iinclude
LDFLAGS := $(TARGET_FLAGS) $(LINKER_SELECT) -nostdlib -flto \
    -Wl,--build-id=none -Wl,--gc-sections -Wl,-T,linker/kernel8.ld \
    -Wl,-Map,$(BUILD)/kernel8.map
OBJECTS := $(addprefix $(BUILD)/,pacman.o platform.o main.o roms.o start.o)

.PHONY: all clean core roms example-pi4 FORCE
all: $(BUILD)/kernel8.img
core: $(CORE)
roms: $(GENERATED)/roms.c
FORCE:

# Generators validate their inputs on every invocation, but replace outputs only
# when bytes change. This also detects ROM_DIR changes without stale ROM objects.
$(CORE): FORCE
	$(PYTHON) $(STANDALONE)/tools/generate_pacman.py

$(GENERATED)/roms.c: FORCE
	$(PYTHON) scripts/pack_roms.py "$(ROM_DIR)" "$@"

$(BUILD):
	mkdir -p "$@"

# Changes to compiler selection or user-provided flags invalidate existing objects.
$(BUILD)/build-options.txt: FORCE | $(BUILD)
	@{ printf '%s\n' '$(CC)' '$(OBJCOPY)' '$(CFLAGS)' '$(LDFLAGS)'; } > "$@.tmp"
	@cmp -s "$@.tmp" "$@" && rm "$@.tmp" || mv "$@.tmp" "$@"

$(OBJECTS): $(BUILD)/build-options.txt
$(BUILD)/pacman.o: $(CORE) $(STANDALONE)/include/pacman.h $(STANDALONE)/include/platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/platform.o: src/platform.c include/raspberry_pi.h $(STANDALONE)/include/platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/main.o: src/main.c $(STANDALONE)/include/pacman.h $(STANDALONE)/include/platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/roms.o: $(GENERATED)/roms.c $(STANDALONE)/include/pacman.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/start.o: boot/start.S | $(BUILD)
	$(CC) $(TARGET_FLAGS) -mcpu=$(CPU) -c $< -o $@
$(BUILD)/kernel8.elf: $(OBJECTS) linker/kernel8.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
$(BUILD)/kernel8.img: $(BUILD)/kernel8.elf
	$(OBJCOPY) -O binary $< $@
	@printf 'Built %s for Raspberry Pi MODEL=%s with %s\n' "$@" "$(MODEL)" "$(TOOLCHAIN)"

# Keep the supplied example untouched unless this explicit target is requested.
example-pi4:
	$(MAKE) MODEL=4 TOOLCHAIN=$(TOOLCHAIN) all
	mkdir -p "$(EXAMPLE)"
	cp "$(ROOT)/build/standalone/raspberry_pi/model4/$(TOOLCHAIN)/kernel8.img" "$(EXAMPLE)/kernel8.img"
	cp boot/config.txt "$(EXAMPLE)/config.txt"
	cd "$(EXAMPLE)" && sha256sum kernel8.img config.txt > SHA256SUMS

clean:
	rm -rf "$(BUILD)"

-include $(OBJECTS:.o=.d)
