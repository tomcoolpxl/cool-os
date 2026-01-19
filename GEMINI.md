# Gemini Context: cool-os

This document provides essential context for working on the `cool-os` project.

## Project Overview

`cool-os` is a teaching-oriented, monolithic x86-64 kernel developed for educational purposes. The project emphasizes clarity, debuggability, and incremental development. It is designed to boot via UEFI using the Limine bootloader and run on QEMU with KVM acceleration, with future compatibility for real hardware in mind.

The kernel is written primarily in C, with some assembly for low-level tasks like context switching and interrupt handling. It features a higher-half kernel memory layout, a physical memory manager (PMM), a kernel heap, a cooperative scheduler, support for user-mode applications (loaded as ELF files), a VFS layer with a FAT32 driver, and basic graphics support via a framebuffer console.

A comprehensive development log and feature list can be found in `CLAUDE.md`. The project's development is broken down into prototypes, with requirements for each stage detailed in `REQUIREMENTS__PROTO.md`.

## Building and Running

The project uses a unified `Makefile`-based build system with three build flavors.

### Build Flavors

*   **debug** (default): Development build with `-O2 -g -DDEBUG`
*   **release**: Production build with `-O3` and stripped symbols
*   **test**: Test build with `-Og -g -DTEST_BUILD`, includes `tests/*.c`
*   **regtest**: Automated test build with `-Og -g -DREGTEST_BUILD`, includes `tests/regtest_suites.c`

### Build Commands

*   **Build the default (debug) flavor:**
    ```bash
    make
    ```

*   **Build a specific flavor:**
    ```bash
    make release    # Production build
    make test       # Test build
    ```

*   **Run automated regression tests (CI-friendly):**
    ```bash
    make regtest        # Build and run all tests
    make regtest-build  # Build only, don't run
    ```
    Exit codes: 0 = pass, 1 = fail or timeout

*   **Build and run in QEMU:**
    ```bash
    make run            # Run debug flavor
    make run-release    # Run release flavor
    make run-test       # Run test flavor
    ```

*   **Create a bootable USB image:**
    ```bash
    make image
    ```
    This creates `build/dist/cool-os-release.img` suitable for writing to a USB stick.

*   **Clean build artifacts:**
    ```bash
    make clean
    ```

### Build Artifacts

*   `build/dist/kernel-<flavor>.elf` - Kernel ELF binary
*   `build/dist/cool-os-<flavor>.img` - Bootable FAT32 image
*   `build/dist/user/*.elf` - User program ELF files
*   `build/obj/<flavor>/` - Object files per flavor

## Development Conventions

### Toolchain and Architecture

*   **Architecture**: x86-64 (Long Mode)
*   **Bootloader**: Limine (UEFI)
*   **Compiler**: `gcc`
*   **Linker**: `ld`
*   **Emulator**: `qemu-system-x86_64`

### Code Structure

*   `src/`: Kernel source files (C and Assembly).
    *   `kernel.c`: The main kernel entry point (`kmain`).
*   `include/`: Kernel header files.
*   `user/`: User-space applications (Assembly).
*   `tests/`: Kernel test suite (`kernel_tests.c`), included in test flavor builds.
*   `linker.ld`: Kernel linker script, placing the kernel in the higher half (`0xFFFFFFFF80000000`).
*   `user/user.ld`: User-space linker script, placing programs at `0x400000`.
*   `limine.conf`: Limine bootloader configuration.

### Key Documents

For a much more detailed understanding of the project's features, architecture, and design philosophy, please refer to **`CLAUDE.md`**. This file is the most comprehensive source of information for this repository.

## Automated Regression Testing

The `make regtest` target runs automated tests in QEMU without human intervention, suitable for CI pipelines.

### Features
- Uses QEMU's `isa-debug-exit` device for programmatic exit codes
- 60-second timeout (configurable via `REGTEST_TIMEOUT` environment variable)
- Serial output logged to `build/regtest.log`
- Tests PMM, heap, tasks, user mode, ELF loading, filesystem, framebuffer, and console

### Usage
```bash
make regtest    # Run all tests
echo $?         # Check exit code (0=pass, 1=fail)
cat build/regtest.log | grep "\[REGTEST\]"  # View test results
```

See `CLAUDE.md` for complete documentation.
