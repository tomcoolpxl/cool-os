# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

cool-os is a teaching-oriented x86-64 monolithic kernel prototype. The primary goal is debuggability, reproducibility, and incremental extensibility for educational purposes.

**Current Status:** Specification phase (Proto 0). See `REQUIREMENTS__PROTO.md` for the authoritative requirements.

## Target Architecture

- x86-64 (AMD64) in long mode
- UEFI boot via Limine bootloader (no legacy BIOS)
- Primary execution: QEMU with KVM acceleration
- Secondary: Real x86-64 UEFI hardware

## Build Commands

Once implemented, the build system will use Make:

```bash
make        # Build kernel.elf and esp.img
make run    # Build and launch in QEMU
```

## Planned Build Artifacts

- `kernel.elf` - ELF64 kernel image
- `esp.img` - FAT32 UEFI bootable partition containing:
  - `EFI/BOOT/BOOTX64.EFI` (Limine)
  - `limine.cfg`
  - Kernel binary

## Toolchain

- C (freestanding) with optional x86-64 assembly
- x86_64-elf cross compiler or system compiler with freestanding flags
- GNU ld or lld linker

## QEMU Requirements

- OVMF_CODE.fd (read-only UEFI firmware)
- OVMF_VARS.fd (writable copy per VM)
- KVM acceleration enabled
- Serial redirected to host terminal

## Proto 0 Scope

**In scope:**
- UEFI boot, long mode entry
- Kernel C code execution
- Serial output (COM1 UART at 0x3F8, polling mode)
- Clean halt via HLT instruction

**Explicitly out of scope for Proto 0:**
- Custom bootloader, SMP, ACPI, networking, filesystems
- User mode, memory allocators, interrupt controller beyond basic exceptions, GPU drivers

## Design Philosophy

- Simplicity over abstraction
- Deterministic behavior for teaching/debugging
- No premature optimization
- Standard PC platform only (no hardware-specific dependencies)
