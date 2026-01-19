# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

cool-os is a teaching-oriented x86-64 monolithic kernel prototype. The primary goal is debuggability, reproducibility, and incremental extensibility for educational purposes.

**Current Status:** Proto 11 complete (Text Console). See `REQUIREMENTS__PROTO.md` for the authoritative requirements.

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
make test-ud       # Test invalid opcode exception
make test-pf       # Test page fault exception
make test-graphics # Run framebuffer and console tests (Proto 10/11)
```

## Build Artifacts

- `build/kernel.elf` - ELF64 kernel image
- `build/esp.img` - FAT32 UEFI bootable partition containing:
  - `EFI/BOOT/BOOTX64.EFI` (Limine)
  - `limine.conf`
  - Kernel binary
- `build/data.img` - FAT32 data disk containing user ELF programs (INIT.ELF, YIELD1.ELF, etc.)

## Toolchain

- GCC with freestanding flags (`-ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mno-sse -mcmodel=kernel`)
- GNU ld with custom `linker.ld`
- NASM-style assembly via GCC

## QEMU Requirements

- OVMF_CODE.4m.fd (read-only UEFI firmware)
- OVMF_VARS.4m.fd (writable copy per VM)
- KVM acceleration enabled
- Serial redirected to host terminal (`-serial stdio`)
- GTK display for framebuffer output (`-display gtk`)
- Two IDE drives: esp.img (boot, index 0) and data.img (data, index 1)

## Implemented Features (Proto 1-11)

### Proto 1: Boot & Serial
- UEFI boot via Limine, long mode entry
- Higher-half kernel mapping (HHDM)
- Serial output (COM1 UART at 0x3F8, polling mode)
- IDT with exception handlers (#DE, #DB, #BP, #UD, #GP, #PF, etc.)

### Proto 2: Physical Memory Manager
- Bitmap-based PMM tracking all physical frames
- `pmm_alloc_frame()` / `pmm_free_frame()` API
- `pmm_alloc_frames_contiguous(count)` for multi-page allocations
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

### Proto 5: Time Services (Sleep and Delay)
- `timer_get_ticks()` API for reading tick count
- `timer_sleep_ticks(n)` blocking delay using `hlt` loop
- `timer_sleep_ms(n)` millisecond-level delay with rounding
- `TIMER_HZ` constant (100 Hz)
- Lightweight IRQ handler (no serial output)

### Proto 6: Cooperative Multitasking
- Task structure with 4 KiB stacks (1 PMM frame per task)
- `task_create(entry)` / `task_yield()` / `task_current()` API
- Round-robin scheduler with circular linked list
- Assembly context switch saving callee-saved registers (RBP, RBX, R12-R15)
- Bootstrap task representing kmain's context
- Idle task running `hlt` loop when no tasks are ready
- Task states: RUNNING, READY, FINISHED

### Proto 7: User Mode and System Calls
- Custom GDT with kernel (DPL=0) and user (DPL=3) segments
- TSS for RSP0 stack switching on ring transitions
- SYSCALL/SYSRET via MSR configuration (STAR, LSTAR, FMASK)
- `task_create_user(code, code_size)` creates ring 3 tasks from raw machine code
- User code/stack mapped at low canonical addresses (0x400000+, 0x800000+)
- 4-level page table mapping with proper U/S bit for user-space pages
- NX (No-Execute) bit set on user data/stack pages; code pages remain executable
- System calls: `SYS_exit`, `SYS_write`, `SYS_yield`
- User-mode syscall wrappers: `user_exit()`, `user_write()`, `user_yield()`
- User fault isolation (faults kill task, kernel survives)
- Kernel higher-half pages remain supervisor-only (U/S=0)

### Proto 8: ELF64 Loader
- ELF64 executable loader for real user programs
- Limine module support for loading user programs from boot image
- `task_create_elf(data, size)` creates user tasks from ELF files
- `elf_load_at()` supports loading at unique addresses (multi-program support)
- User programs linked at 0x400000, relocated to 0x1000000+ for isolation
- User stack at 0x70000000 (16 KiB, NX protected)
- Validates: ELF magic, x86-64, PT_LOAD segments, user address range
- Maps segments with correct permissions: R/W/X → U/S=1, NX appropriately

### Proto 9: Filesystem and Disk-Backed Loading
- ATA PIO driver for IDE disk access (ports 0x1F0-0x1F7, LBA28 addressing)
- FAT32 filesystem driver (read-only, superfloppy layout, 8.3 filenames)
- VFS layer with file descriptor table (16 slots)
- `task_create_from_path(path)` loads ELF from disk (e.g., "INIT.ELF")
- Block device interface: `block_init()`, `block_read(lba, count, dst)`
- FAT32 API: `fat_mount()`, `fat_open()`, `fat_read()`, `fat_seek()`, `fat_close()`, `fat_get_size()`
- VFS API: `vfs_init()`, `vfs_open()`, `vfs_read()`, `vfs_seek()`, `vfs_close()`, `vfs_size()`
- Heap enhanced to support multi-page allocations for larger files
- Data disk (data.img) attached as IDE slave drive

### Proto 10: Framebuffer Graphics
- Limine framebuffer request for UEFI GOP access
- Double-buffered rendering (4MB back buffer in RAM, bulk copy to VRAM)
- Native resolution rendering (uses firmware-provided resolution, e.g., 1280x800)
- 32-bit XRGB pixel format
- Drawing primitives: `fb_putpixel()`, `fb_clear()`, `fb_fill_rect()`, `fb_present()`
- `fb_get_info()` returns framebuffer metadata (dimensions, pitch, addresses)
- Graceful fallback to direct rendering if back buffer allocation fails
- QEMU display mode changed from `-display none` to `-display gtk`

### Proto 11: Text Console
- Software-rendered text console using framebuffer
- Embedded 8x16 VGA bitmap font (256 characters, 4KB)
- Console API: `console_init()`, `console_putc()`, `console_puts()`, `console_clear()`
- Character grid calculated from framebuffer dimensions (e.g., 160x50 at 1280x800)
- Automatic scrolling via optimized memmove on back buffer
- Special character handling: `\n`, `\r`, `\t`, `\b`
- Panic messages displayed on both console and serial
- White text on black background (configurable colors internally)

## User Programs

User programs are in `user/` directory:
- `init.S` - Hello world (prints message, exits)
- `yield1.S` / `yield2.S` - Yield test (alternating output)
- `fault.S` - Privilege separation test (jumps to kernel address)

Build: User programs compiled to ELF64 via `user/user.ld`, included as Limine modules.

## Source Structure

```
include/
  block.h       - Block device interface (ATA PIO)
  console.h     - Text console API
  cpu.h         - CPU control (read CR2/CR3, halt)
  elf.h         - ELF64 structures and loader API
  fat32.h       - FAT32 filesystem structures and API
  framebuffer.h - Framebuffer graphics API
  gdt.h         - GDT structures and segment selectors
  heap.h        - Heap API (kmalloc/kfree)
  hhdm.h        - Higher-half direct map helpers
  idt.h         - IDT structures and init
  isr.h         - Interrupt frame and handler declarations
  limine.h      - Limine bootloader protocol
  msr.h         - Model-Specific Register access (rdmsr/wrmsr)
  paging.h      - Page table manipulation
  panic.h       - ASSERT macro and panic()
  pic.h         - 8259A PIC driver API
  pit.h         - 8253/8254 PIT driver API
  pmm.h         - Physical memory manager API
  ports.h       - I/O port access (inb/outb/inw/outw/insw/io_wait)
  scheduler.h   - Scheduler API (init/add/yield)
  serial.h      - Serial port I/O
  syscall.h     - Syscall numbers and dispatcher
  task.h        - Task API (create/create_user/create_elf/create_from_path/yield/current)
  timer.h       - Timer subsystem API (sleep/delay functions)
  user.h        - User-mode syscall wrappers
  vfs.h         - Virtual filesystem API

src/
  block.c       - ATA PIO driver implementation
  console.c     - Text console with embedded 8x16 font
  context_switch.S - Assembly context switch routine
  elf.c         - ELF64 loader implementation
  fat32.c       - FAT32 filesystem driver
  framebuffer.c - Double-buffered framebuffer graphics
  gdt.c         - GDT and TSS initialization
  heap.c        - Arena-based heap (contiguous multi-page support)
  idt.c         - IDT setup
  isr.c         - Exception handlers (with user fault handling)
  isr_stubs.S   - Assembly ISR/IRQ entry points
  kernel.c      - Main kernel entry (kmain)
  paging.c      - User-space page table mapping (4-level paging)
  pic.c         - 8259A PIC driver implementation
  pit.c         - 8253/8254 PIT driver implementation
  pmm.c         - Bitmap PMM with contiguous allocation
  scheduler.c   - Round-robin scheduler implementation
  serial.c      - Serial port driver
  syscall.c     - Syscall initialization and dispatch
  syscall_entry.S - Assembly SYSCALL/SYSRET entry point
  task.c        - Task creation and user/ELF/disk mode support
  timer.c       - Timer services and IRQ handler
  vfs.c         - VFS layer implementation

user/
  init.S      - Hello world user program
  yield1.S    - Yield test program 1
  yield2.S    - Yield test program 2
  fault.S     - Privilege separation test
  user.ld     - Linker script for user programs
```

## Design Philosophy

- Simplicity over abstraction
- Deterministic behavior for teaching/debugging
- No premature optimization
- Standard PC platform only (no hardware-specific dependencies)
