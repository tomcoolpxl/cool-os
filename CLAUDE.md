# CLAUDE.md

## Project Overview

`cool-os` is a teaching-oriented, monolithic x86-64 kernel developed for educational purposes. The project emphasizes clarity, debuggability, and incremental development. It is designed to boot via UEFI using the Limine bootloader and run on QEMU with KVM acceleration, with future compatibility for real hardware in mind.

The kernel is written primarily in C, with some assembly for low-level tasks like context switching and interrupt handling. It features a higher-half kernel memory layout, a physical memory manager (PMM), a kernel heap, a cooperative scheduler, support for user-mode applications (loaded as ELF files), a VFS layer with a FAT32 driver, and basic graphics support via a framebuffer console.

**Implemented:** prototypes in `./DONE/prototype*.md`

**Planned Work:** prototype planning in `./TODO/prototype*.md`. Order according to prototype veersion number.

**Current Status:** Proto 12 (Keyboard Input) Complete. Using PS/2 driver (with USB legacy emulation). Native XHCI disabled. See `DONE/usb_debug_plan.md` for details.

## Target Architecture

- x86-64 (AMD64) in long mode
- UEFI boot via Limine bootloader (no legacy BIOS)
- Primary execution: QEMU with KVM acceleration
- Secondary: Real x86-64 UEFI hardware

### Toolchain and Architecture

*   **Architecture**: x86-64 (Long Mode)
*   **Bootloader**: Limine (UEFI)
*   **Compiler**: `gcc`
*   **Linker**: `ld`
*   **Emulator**: `qemu-system-x86_64`

## Build Commands

The unified Makefile supports four build flavors: debug (default), release, test, and regtest.

```bash
make                # Build debug flavor (default)
make release        # Build release flavor (optimized, stripped)
make test           # Build test flavor (includes tests/*.c)
make regtest        # Build and run automated regression tests (CI-friendly)
make regtest-build  # Build regtest flavor without running
make run            # Build and run debug flavor in QEMU
make run-release    # Build and run release flavor in QEMU
make run-test       # Build and run test flavor in QEMU
make image          # Create bootable USB image from release build
make clean          # Remove all build artifacts
```

Build flavor differences:
- **debug**: `-O2 -g -DDEBUG` (development)
- **release**: `-O3`, stripped symbols (production)
- **test**: `-Og -g -DTEST_BUILD`, includes `tests/*.c` (interactive testing)
- **regtest**: `-Og -g -DREGTEST_BUILD`, includes `tests/regtest_suites.c` (automated testing)

## Build Artifacts

- `build/dist/kernel-<flavor>.elf` - ELF64 kernel image
- `build/dist/cool-os-<flavor>.img` - FAT32 UEFI bootable image containing:
  - `EFI/BOOT/BOOTX64.EFI` (Limine)
  - `limine.conf`
  - Kernel binary
  - User program ELFs
- `build/dist/user/*.elf` - User program ELF files
- `build/obj/<flavor>/` - Object files per flavor

## Implemented Features (Proto 1-12)

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
- ASCII translation with shift and caps lock support
- 256-byte ring buffer for asynchronous input
- Keyboard API: `kbd_init()`, `kbd_getc_nonblock()`, `kbd_getc_blocking()`, `kbd_readline()`
- Line editing with backspace support and console echo
- Blocking input via `hlt` loop (interrupt-driven, no busy-wait)
- Modifier key tracking: left/right shift, caps lock, ctrl

### Proto 12: Keyboard Input (PS/2 + Experimental USB)
- **Primary:** PS/2 Keyboard Driver (8042 Controller)
  - IRQ1 handling, scancode Set 1 decoding
  - `kbd_getc_blocking()` / `kbd_readline()` API
  - Input ring buffer with concurrent access protection
- **Secondary (DISABLED):** Native USB XHCI Driver
  - PCI enumeration and BAR mapping (Code present but disabled)
  - BIOS-to-OS Handoff logic (Implemented in `xhci.c`)
- **Fallback Strategy:** System relies on BIOS Legacy Emulation (PS/2) for broad compatibility.

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
  console.h     - Text console API (putc/puts/clear/erase_char)
  cpu.h         - CPU control (read CR2/CR3, halt)
  elf.h         - ELF64 structures and loader API
  fat32.h       - FAT32 filesystem structures and API
  framebuffer.h - Framebuffer graphics API
  gdt.h         - GDT structures and segment selectors
  heap.h        - Heap API (kmalloc/kfree)
  hhdm.h        - Higher-half direct map helpers
  idt.h         - IDT structures and init
  isr.h         - Interrupt frame and handler declarations
  kbd.h         - PS/2 keyboard driver API
  limine.h      - Limine bootloader protocol
  msr.h         - Model-Specific Register access (rdmsr/wrmsr)
  paging.h      - Page table manipulation
  panic.h       - ASSERT macro and panic()
  pic.h         - 8259A PIC driver API
  pit.h         - 8253/8254 PIT driver API
  pmm.h         - Physical memory manager API
  ports.h       - I/O port access (inb/outb/inw/outw/insw/io_wait)
  regtest.h     - Regression test infrastructure API
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
  isr_stubs.S   - Assembly ISR/IRQ entry points (vectors 0-31, 0x20, 0x21)
  kbd.c         - PS/2 keyboard driver with scancode translation
  kernel.c      - Main kernel entry (kmain)
  paging.c      - User-space page table mapping (4-level paging)
  pic.c         - 8259A PIC driver implementation
  pit.c         - 8253/8254 PIT driver implementation
  pmm.c         - Bitmap PMM with contiguous allocation
  regtest.c     - Regression test infrastructure implementation
  scheduler.c   - Round-robin scheduler implementation
  serial.c      - Serial port driver
  syscall.c     - Syscall initialization and dispatch
  syscall_entry.S - Assembly SYSCALL/SYSRET entry point
  task.c        - Task creation and user/ELF/disk mode support
  timer.c       - Timer services and IRQ dispatcher (timer + keyboard)
  vfs.c         - VFS layer implementation

user/
  init.S      - Hello world user program
  yield1.S    - Yield test program 1
  yield2.S    - Yield test program 2
  fault.S     - Privilege separation test
  user.ld     - Linker script for user programs
prototype13.md prototype14.md prototype15.md prototype16.md prototype17.md prototype18.md prototype19.md prototype20.md prototype21.md prototype22.md prototype23.md prototype24.md prototype25.md prototype26.md prototype27.md prototype28.md prototype29.md prototype30.md prototype31.md prototype32.md prototype33.md prototype34.md prototype35.md prototype36.md prototype37.md prototype38.md prototype39.md prototype40.md
tests/
  kernel_tests.c   - Interactive test suite (included in test flavor via TEST_BUILD)
  regtest_suites.c - Automated regression test suites (included in regtest flavor)

scripts/
  run_regtest.sh - QEMU runner script for regression tests
```

## Design Philosophy

- Simplicity over abstraction
- Deterministic behavior for teaching/debugging
- No premature optimization
- Standard PC platform only (no hardware-specific dependencies)

## Automated Regression Testing

The `regtest` build flavor provides CI-friendly automated testing that runs in QEMU without human intervention and exits with appropriate exit codes.

### Running Tests

```bash
make regtest        # Build and run all regression tests
echo $?             # 0 = pass, 1 = fail
```

### How It Works

1. **QEMU Exit Mechanism**: Uses `isa-debug-exit` device (port 0x501)
   - Write `0x00` → QEMU exits with code 1 (tests passed)
   - Write `0x01` → QEMU exits with code 3 (tests failed)

2. **Test Runner Script**: `scripts/run_regtest.sh`
   - Runs QEMU with 60-second timeout (configurable via `REGTEST_TIMEOUT`)
   - Captures serial output to `build/regtest.log`
   - Parses `[REGTEST]` prefixed output for pass/fail counts

3. **Output Format**:
   ```
   [REGTEST] START suite_name
   [REGTEST] PASS test_name
   [REGTEST] FAIL test_name: reason
   [REGTEST] END suite_name passed=N failed=M
   [REGTEST] SUMMARY total=N passed=P failed=F
   [REGTEST] EXIT code
   ```

### Test Suites

| Suite | Description |
|-------|-------------|
| `pmm` | Physical Memory Manager: alloc/free, patterns, contiguous |
| `heap` | Kernel Heap: kmalloc/kfree, coalescing, stress test |
| `task` | Cooperative Multitasking: create, switch, exit |
| `user` | User Mode: syscalls, yield, fault isolation |
| `elf` | ELF Loader: module loading, task creation |
| `fs` | Filesystem: VFS open/read/seek/close, disk ELF loading |
| `fb` | Framebuffer: init, dimensions, clear, fill, present |
| `console` | Text Console: init, putc, puts, scroll |

### Exit Codes

| QEMU Exit | Script Exit | Meaning |
|-----------|-------------|---------|
| 1 | 0 | All tprototype13.md prototype14.md prototype15.md prototype16.md prototype17.md prototype18.md prototype19.md prototype20.md prototype21.md prototype22.md prototype23.md prototype24.md prototype25.md prototype26.md prototype27.md prototype28.md prototype29.md prototype30.md prototype31.md prototype32.md prototype33.md prototype34.md prototype35.md prototype36.md prototype37.md prototype38.md prototype39.md prototype40.mdeout (hung or infinite loop) |
| Other | 1 | Unexpected error |

### API (`include/regtest.h`)

```c
void regtest_exit(int success);           // Exit QEMU with result
void regtest_log(const char *fmt, ...);   // Log with [REGTEST] prefix
void regtest_pass(const char *test_name); // Record passing test
void regtest_fail(const char *test_name, const char *reason); // Record failure
void regtest_start_suite(const char *suite_name);  // Start suite
void regtest_end_suite(const char *suite_name);    // End suite
int regtest_run_all(void);                // Run all enabled suites
```