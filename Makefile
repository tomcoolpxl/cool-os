# cool-os Proto 1 Build System

CC := gcc
AS := gcc
LD := ld

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
          -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -O2 \
          -I include

ASFLAGS := -ffreestanding -fno-pic -mno-red-zone -mcmodel=kernel

LDFLAGS := -nostdlib -static -T linker.ld

OVMF_CODE := /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS := /usr/share/edk2/x64/OVMF_VARS.4m.fd

LIMINE_VERSION := 8.6.0
LIMINE_URL := https://github.com/limine-bootloader/limine/raw/v$(LIMINE_VERSION)-binary/BOOTX64.EFI

C_SRCS := src/kernel.c src/serial.c src/gdt.c src/idt.c src/isr.c src/pmm.c src/heap.c src/pic.c src/pit.c src/timer.c src/task.c src/scheduler.c src/syscall.c src/paging.c src/elf.c
ASM_SRCS := src/isr_stubs.S src/context_switch.S src/syscall_entry.S
C_OBJS := $(C_SRCS:src/%.c=build/%.o)
ASM_OBJS := $(ASM_SRCS:src/%.S=build/%.o)
OBJS := $(C_OBJS) $(ASM_OBJS)

# User programs
USER_SRCS := user/init.S user/yield1.S user/yield2.S user/fault.S
USER_ELFS := $(USER_SRCS:user/%.S=build/user/%.elf)

.PHONY: all clean run limine test-ud test-pf user

all: build/kernel.elf $(USER_ELFS)

user: $(USER_ELFS)

build/kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.S
	@mkdir -p build
	$(AS) $(ASFLAGS) -c $< -o $@

# User program build rules
build/user/%.o: user/%.S
	@mkdir -p build/user
	$(AS) -ffreestanding -fno-pic -c $< -o $@

build/user/%.elf: build/user/%.o user/user.ld
	$(LD) -nostdlib -static -T user/user.ld -o $@ $<

limine: build/BOOTX64.EFI include/limine.h

build/BOOTX64.EFI:
	@mkdir -p build
	curl -L -o $@ $(LIMINE_URL)

include/limine.h:
	@mkdir -p include
	curl -L -o $@ https://github.com/limine-bootloader/limine/raw/v$(LIMINE_VERSION)-binary/limine.h

esp.img: build/esp.img

build/esp.img: build/kernel.elf build/BOOTX64.EFI limine.conf $(USER_ELFS)
	@mkdir -p build
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.fat -F 32 $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ build/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@ limine.conf ::/limine.conf
	mcopy -i $@ build/kernel.elf ::/kernel.elf
	mcopy -i $@ build/user/init.elf ::/init.elf
	mcopy -i $@ build/user/yield1.elf ::/yield1.elf
	mcopy -i $@ build/user/yield2.elf ::/yield2.elf
	mcopy -i $@ build/user/fault.elf ::/fault.elf

build/OVMF_VARS.4m.fd: $(OVMF_VARS)
	@mkdir -p build
	cp $(OVMF_VARS) $@

run: build/esp.img build/OVMF_VARS.4m.fd
	qemu-system-x86_64 \
		-enable-kvm \
		-cpu host \
		-m 256M \
		-no-reboot \
		-no-shutdown \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=build/OVMF_VARS.4m.fd \
		-drive format=raw,file=build/esp.img \
		-serial stdio \
		-display none

# Test targets: trigger specific exceptions
test-ud:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DTEST_UD" all
	$(MAKE) run

test-pf:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DTEST_PF" all
	$(MAKE) run

clean:
	rm -rf build
