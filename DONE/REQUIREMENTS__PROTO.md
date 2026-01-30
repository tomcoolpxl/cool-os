```md
# Requirements Specification: Minimal x86-64 Teaching OS Prototype

## Purpose

Define the smallest clean prototype for a teaching-oriented x86-64 monolithic kernel that boots under QEMU with KVM acceleration and can later boot on real UEFI hardware.

The prototype must prioritize debuggability, reproducibility, and incremental extensibility.

## Scope

This specification covers only the initial bootstrap prototype (Prototype 0). It does not include multitasking, filesystems, userland, or graphics subsystems beyond basic framebuffer access.

## Goals

- Boot reliably using UEFI firmware.
- Enter 64-bit long mode.
- Execute kernel C code.
- Provide visible output for debugging.
- Halt cleanly without crashes.
- Support fast iteration using QEMU with KVM.

## Non-Goals

The following are explicitly out of scope for this prototype:

- Writing a custom UEFI bootloader.
- SMP support.
- ACPI parsing.
- Networking.
- Filesystems.
- User mode processes.
- Memory allocators.
- Interrupt controller configuration beyond basic exceptions.
- GPU drivers.

## Target Platform

### Architecture

- x86-64 (AMD64)
- Long mode enabled before kernel entry.

### Firmware

- UEFI (OVMF under QEMU).
- No legacy BIOS support required.

### Execution Environment

Primary:
- QEMU with KVM acceleration on Linux host.

Secondary:
- Real x86-64 UEFI hardware (future compatibility goal).

## Boot Method

### Bootloader

- Use Limine UEFI bootloader.
- Bootloader responsibilities:
  - Load kernel ELF64.
  - Provide memory map.
  - Provide framebuffer information (optional for prototype 0).
  - Transfer control to kernel entry point in long mode.

### Kernel Entry

- Kernel entry point implemented in C with optional small assembly stub.
- Kernel receives bootloader-provided structures as defined by Limine protocol.

## Kernel Requirements

### Language

- Primary: C (freestanding)
- Optional: minimal x86-64 assembly for entry setup.

### Binary Format

- ELF64 kernel image.
- Linked with custom linker script.

### Execution State at Entry

Kernel must assume:

- CPU in long mode.
- Paging enabled.
- Interrupts disabled.
- Valid stack available or immediately initialized by kernel.

### Mandatory Kernel Capabilities

#### Serial Output

- Initialize COM1 UART at I/O port 0x3F8.
- Support byte output via polling.
- Provide simple print function for debugging.

#### Visible Output

At least one of:

- Serial console output (mandatory).
- Framebuffer pixel output (optional for Prototype 0).

#### CPU Halt

- After printing success message, kernel enters safe halt loop using HLT.

### Stability Requirements

- No triple faults.
- No undefined memory accesses.
- No reliance on uninitialized data.

## Disk and Image Layout

### EFI System Partition (ESP)

Must contain:

- EFI/BOOT/BOOTX64.EFI (Limine UEFI executable)
- limine.cfg
- Kernel ELF64 binary

Filesystem format:

- FAT32.

## Build System

### Toolchain

- x86_64-elf cross compiler or system compiler with freestanding flags.
- GNU ld or lld linker.
- Make-based build system (or equivalent).

### Build Outputs

- kernel.elf
- esp.img (UEFI bootable FAT image)

### Build Requirements

- Single command build and run workflow:
  - Example: make run

## QEMU Runtime Requirements

### Firmware

- OVMF_CODE.fd (read-only)
- OVMF_VARS.fd (writable copy per VM)

### Execution Flags

Required:

- KVM acceleration enabled.
- Serial redirected to host terminal.

Example capabilities required:

- Serial console output to stdio.
- Graphical window optional.

### Performance

- Must run with hardware virtualization.
- No software-only emulation dependency.

## Debugging Requirements

### Mandatory

- Serial console available on boot.
- Deterministic boot behavior.

### Optional (Future)

- GDB stub support.
- Symbolized kernel builds.

## Prototype Completion Criteria

Prototype 0 is considered complete when:

- QEMU boots successfully using UEFI firmware.
- Kernel executes and prints a visible message.
- Kernel halts cleanly.
- Rebooting the VM produces identical behavior.
- Build and run workflow is automated.

## Next Milestone After Prototype 0

Prototype 1 goals (not part of this spec but planned):

- IDT installation.
- Exception handlers with serial output.
- Timer interrupt.
- Basic panic handler.

These are required before implementing scheduling or memory management.

## Design Constraints

- Prefer simplicity over abstraction.
- Avoid premature optimization.
- Avoid hardware-specific dependencies beyond standard PC platform.
- Favor deterministic behavior for teaching and debugging.

## Compliance

An implementation complies with this specification if all mandatory requirements in this document are satisfied and Prototype Completion Criteria are met.
```
