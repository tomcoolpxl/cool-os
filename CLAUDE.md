# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

cool-os is a teaching-oriented x86-64 monolithic kernel prototype. The primary goal is debuggability, reproducibility, and incremental extensibility for educational purposes.

**Current Status:** Proto 4 complete (timer interrupts). See `REQUIREMENTS__PROTO.md` for the authoritative requirements.

## Target Architecture

- x86-64 (AMD64) in long mode
- UEFI boot via Limine bootloader (no legacy BIOS)
- Primary execution: QEMU with KVM acceleration
- Secondary: Real x86-64 UEFI hardware

## Build Commands

```bash
make        # Build kernel.elf
make run    # Build esp.img and launch in QEMU
make clean  # Remove build artifacts
make test-ud  # Test invalid opcode exception
make test-pf  # Test page fault exception
```

## Build Artifacts

- `build/kernel.elf` - ELF64 kernel image
- `build/esp.img` - FAT32 UEFI bootable partition containing:
  - `EFI/BOOT/BOOTX64.EFI` (Limine)
  - `limine.conf`
  - Kernel binary

## Toolchain

- GCC with freestanding flags (`-ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mno-sse -mcmodel=kernel`)
- GNU ld with custom `linker.ld`
- NASM-style assembly via GCC

## QEMU Requirements

- OVMF_CODE.4m.fd (read-only UEFI firmware)
- OVMF_VARS.4m.fd (writable copy per VM)
- KVM acceleration enabled
- Serial redirected to host terminal (`-serial stdio`)

## Implemented Features (Proto 1-4)

### Proto 1: Boot & Serial
- UEFI boot via Limine, long mode entry
- Higher-half kernel mapping (HHDM)
- Serial output (COM1 UART at 0x3F8, polling mode)
- IDT with exception handlers (#DE, #DB, #BP, #UD, #GP, #PF, etc.)

### Proto 2: Physical Memory Manager
- Bitmap-based PMM tracking all physical frames
- `pmm_alloc_frame()` / `pmm_free_frame()` API
- Memory map parsing from Limine
- Double-free detection via ASSERT

### Proto 3: Kernel Heap
- Arena-based allocator (4 KiB pages from PMM)
- `kmalloc(size)` / `kfree(ptr)` API
- First-fit allocation with block splitting
- Coalescing of adjacent free blocks
- 16-byte payload alignment
- Magic constants and double-free detection

### Proto 4: Timer Interrupts (PIT + PIC)
- 8259A PIC driver with IRQ remapping (IRQ0-7 → 0x20-0x27, IRQ8-15 → 0x28-0x2F)
- 8253/8254 PIT driver at 100 Hz
- IRQ stubs with `iretq` return path (separate from exception stubs that halt)
- `pit_get_ticks()` API for tick counting
- Timer prints "tick=N" every 100 ticks (1 second)

## Source Structure

```
include/
  heap.h      - Heap API (kmalloc/kfree)
  hhdm.h      - Higher-half direct map helpers
  idt.h       - IDT structures and init
  isr.h       - Interrupt frame and handler declarations
  limine.h    - Limine bootloader protocol
  panic.h     - ASSERT macro and panic()
  pic.h       - 8259A PIC driver API
  pit.h       - 8253/8254 PIT driver API
  pmm.h       - Physical memory manager API
  ports.h     - I/O port access (inb/outb/io_wait)
  serial.h    - Serial port I/O
  timer.h     - Timer subsystem API

src/
  heap.c      - Arena-based heap implementation
  idt.c       - IDT setup
  isr.c       - Exception handlers
  isr_stubs.S - Assembly ISR/IRQ entry points
  kernel.c    - Main kernel entry (kmain)
  pic.c       - 8259A PIC driver implementation
  pit.c       - 8253/8254 PIT driver implementation
  pmm.c       - Bitmap PMM implementation
  serial.c    - Serial port driver
  timer.c     - Timer IRQ handler
```

## Design Philosophy

- Simplicity over abstraction
- Deterministic behavior for teaching/debugging
- No premature optimization
- Standard PC platform only (no hardware-specific dependencies)
