# cool-os Unified Build System
#
# This Makefile handles four build flavors:
# - debug (default): For development, with -O2 and debug symbols.
# - release: For production, with -O3 and stripped symbols.
# - test: For testing, with test code included and debug-friendly optimization.
# - regtest: For automated regression testing with QEMU exit codes.
#
# Targets:
# - make [all]: Build the default (debug) flavor.
# - make release: Build the release flavor.
# - make test: Build the test flavor.
# - make run: Run the default flavor in QEMU.
# - make run-release: Run the release flavor in QEMU.
# - make run-test: Run the test flavor in QEMU.
# - make regtest: Build and run automated regression tests.
# - make regtest-build: Build regtest flavor without running.
# - make image: Create a bootable USB image from the release build.
# - make clean: Remove all build artifacts.

# --- 1. Configuration ---

# Default flavor if not specified
FLAVOR ?= debug

# Tools
CC := gcc
AS := gcc
LD := ld
STRIP := strip

# Source files
C_SRCS := $(wildcard src/*.c)
ASM_SRCS := $(wildcard src/*.S)
TEST_C_SRCS := $(wildcard tests/*.c)
USER_SRCS := $(wildcard user/*.S)

# Build directories
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj/$(FLAVOR)
USER_OBJ_DIR := $(BUILD_DIR)/user_obj
DIST_DIR := $(BUILD_DIR)/dist

# Output files
KERNEL_ELF := $(DIST_DIR)/kernel-$(FLAVOR).elf
OS_IMG := $(DIST_DIR)/cool-os-$(FLAVOR).img
USER_ELFS := $(patsubst user/%.S,$(DIST_DIR)/user/%.elf,$(USER_SRCS))

# --- 2. Build Flavors ---

# Base flags
CFLAGS_BASE := -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
               -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -I include
ASFLAGS_BASE := -ffreestanding -fno-pic -mno-red-zone
LDFLAGS_BASE := -nostdlib -static -T linker.ld

# Per-flavor flags
ifeq ($(FLAVOR),release)
	CFLAGS := $(CFLAGS_BASE) -O3
	ASFLAGS := $(ASFLAGS_BASE)
	LDFLAGS := $(LDFLAGS_BASE)
else ifeq ($(FLAVOR),test)
	CFLAGS := $(CFLAGS_BASE) -Og -g -DTEST_BUILD -DTEST_GRAPHICS -DTEST_KBD
	ASFLAGS := $(ASFLAGS_BASE) -g
	LDFLAGS := $(LDFLAGS_BASE)
else ifeq ($(FLAVOR),regtest)
	CFLAGS := $(CFLAGS_BASE) -Og -g -DREGTEST_BUILD
	ASFLAGS := $(ASFLAGS_BASE) -g
	LDFLAGS := $(LDFLAGS_BASE)
else # debug
	CFLAGS := $(CFLAGS_BASE) -O2 -g -DDEBUG
	ASFLAGS := $(ASFLAGS_BASE) -g
	LDFLAGS := $(LDFLAGS_BASE)
endif

# Object file lists
C_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst src/%.S,$(OBJ_DIR)/%.o,$(ASM_SRCS))
TEST_OBJS := $(patsubst tests/%.c,$(OBJ_DIR)/%.o,$(TEST_C_SRCS))
USER_OBJS := $(patsubst user/%.S,$(USER_OBJ_DIR)/%.o,$(USER_SRCS))

# Regtest source files
REGTEST_C_SRCS := $(wildcard tests/regtest_suites.c)
REGTEST_OBJS := $(patsubst tests/%.c,$(OBJ_DIR)/%.o,$(REGTEST_C_SRCS))

OBJS := $(C_OBJS) $(ASM_OBJS)
ifeq ($(FLAVOR),test)
	OBJS += $(TEST_OBJS)
endif
ifeq ($(FLAVOR),regtest)
	OBJS += $(REGTEST_OBJS)
endif

# --- 3. Main Targets ---

.PHONY: all clean debug release test regtest regtest-build run run-release run-test image limine-deps

all: $(OS_IMG)

# Flavor-switching targets
debug:
	@$(MAKE) FLAVOR=debug all
release:
	@$(MAKE) FLAVOR=release all
test:
	@$(MAKE) FLAVOR=test all

# Regression test targets
regtest-build:
	@$(MAKE) FLAVOR=regtest all

regtest: regtest-build
	@./scripts/run_regtest.sh

# --- 4. Build Rules ---

# Kernel ELF
$(KERNEL_ELF): $(OBJS) linker.ld
	@mkdir -p $(DIST_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
ifeq ($(FLAVOR),release)
	$(STRIP) --strip-all $@
endif

# Kernel objects
$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: src/%.S
	@mkdir -p $(OBJ_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: tests/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# User programs
$(DIST_DIR)/user/%.elf: $(USER_OBJ_DIR)/%.o user/user.ld
	@mkdir -p $(DIST_DIR)/user
	$(LD) -nostdlib -static -T user/user.ld -o $@ $<

$(USER_OBJ_DIR)/%.o: user/%.S
	@mkdir -p $(USER_OBJ_DIR)
	$(AS) -ffreestanding -fno-pic -c $< -o $@

# --- 5. Image and QEMU ---

LIMINE_EFI := $(BUILD_DIR)/BOOTX64.EFI
LIMINE_URL := https://github.com/limine-bootloader/limine/raw/v8.6.0-binary/BOOTX64.EFI

limine-deps: $(LIMINE_EFI)

$(LIMINE_EFI):
	@mkdir -p $(BUILD_DIR)
	curl -L -o $@ $(LIMINE_URL)

# Unified OS Image
$(OS_IMG): $(KERNEL_ELF) $(USER_ELFS) limine.conf limine-deps
	@mkdir -p $(DIST_DIR)
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.fat -F 32 $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $(LIMINE_EFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@ limine.conf ::/limine.conf
	mcopy -i $@ $(KERNEL_ELF) ::/kernel.elf
	@for user_elf in $(USER_ELFS); do \
		mcopy -i $@ $$user_elf ::/`basename $$user_elf`; \
	done

# QEMU run targets
OVMF_CODE := /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS := $(BUILD_DIR)/OVMF_VARS.4m.fd

run:
	@$(MAKE) FLAVOR=debug run-qemu

run-release:
	@$(MAKE) FLAVOR=release run-qemu

run-test:
	@$(MAKE) FLAVOR=test run-qemu

.PHONY: run-qemu
run-qemu: all
	@if [ ! -f "$(OVMF_VARS)" ]; then cp /usr/share/edk2/x64/OVMF_VARS.4m.fd $(OVMF_VARS); fi
	qemu-system-x86_64 \
		-enable-kvm \
		-cpu host \
		-m 256M \
		-no-reboot \
		-no-shutdown \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS) \
		-drive format=raw,file=$(OS_IMG) \
		-device qemu-xhci \
		-device usb-kbd \
		-serial stdio \
		-display gtk

# Target to create a production USB image
image:
	@$(MAKE) FLAVOR=release all
	@echo "------------------------------------------------------------"
	@echo "Build complete. Image is at: $(DIST_DIR)/cool-os-release.img"
	@echo "To write to a USB stick (e.g., /dev/sdX), run:"
	@echo "sudo dd if=$(DIST_DIR)/cool-os-release.img of=/dev/sdX bs=4M status=progress && sync"
	@echo "------------------------------------------------------------"


# --- 6. Cleanup ---
clean:
	rm -rf $(BUILD_DIR)