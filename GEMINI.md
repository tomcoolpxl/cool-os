# Gemini Context: cool-os

This document provides essential context for working on the `cool-os` project.

## Project Overview

`cool-os` is a teaching-oriented, monolithic x86-64 kernel developed for educational purposes. The project emphasizes clarity, debuggability, and incremental development. It is designed to boot via UEFI using the Limine bootloader and run on QEMU with KVM acceleration, with future compatibility for real hardware in mind.

The kernel is written primarily in C, with some assembly for low-level tasks like context switching and interrupt handling. It features a higher-half kernel memory layout, a physical memory manager (PMM), a kernel heap, a cooperative scheduler, support for user-mode applications (loaded as ELF files), a VFS layer with a FAT32 driver, and basic graphics support via a framebuffer console.

A comprehensive development log and feature list can be found in `CLAUDE.md`. The project's development is broken down into prototypes, with requirements for each stage detailed in `REQUIREMENTS__PROTO.md`.

## Building and Running

The project uses a `Makefile`-based build system.

### Standard Build

*   **Build the kernel and user programs:**
    ```bash
    make all
    ```
    Alternatively, just `make`.

*   **Build and run in QEMU:**
    ```bash
    make run
    ```
    This command compiles the kernel and user programs, creates the necessary disk images (`esp.img` and `data.img`), and launches the OS in a QEMU virtual machine.

*   **Clean build artifacts:**
    ```bash
    make clean
    ```

### Testing

The project includes a separate makefile for running tests. Tests are typically enabled by passing a C preprocessor flag during compilation.

*   **Run a specific test (e.g., test page fault exceptions):**
    ```bash
    make -f Makefile.tests test-pf
    ```

*   **Available test targets** (see `Makefile` for a complete list):
    *   `test-ud`: Test invalid opcode exception.
    *   `test-pf`: Test page fault exception.
    *   `test-graphics`: Run framebuffer and console tests.
    *   `test-kbd`: Run keyboard input tests.

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
*   `tests/`: Kernel tests.
*   `linker.ld`: Kernel linker script, placing the kernel in the higher half (`0xFFFFFFFF80000000`).
*   `user/user.ld`: User-space linker script, placing programs at `0x400000`.
*   `limine.conf`: Limine bootloader configuration.

### Key Documents

For a much more detailed understanding of the project's features, architecture, and design philosophy, please refer to **`CLAUDE.md`**. This file is the most comprehensive source of information for this repository.
