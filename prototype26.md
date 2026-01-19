```md
# Prototype 26: Dynamic Linking and Shared Libraries

## Purpose

Introduce dynamic linking support and shared libraries. This enables programs to load common code at runtime, reduce memory duplication, and prepare the system for scalable userland development.

After this prototype:

- ELF ET_DYN binaries are supported
- Shared libraries can be mapped into processes
- A minimal runtime loader (ld.so equivalent) exists
- libc can be shared across processes
- mmap() becomes the backbone of program loading

## Scope

In scope:
- ELF ET_DYN support
- Dynamic section parsing
- Relocation processing (x86_64 RELA)
- Shared object loading via mmap()
- Runtime loader (userland or kernel-assisted)
- Global symbol resolution (minimal)
- Lazy loading optional

Out of scope:
- Full ELF TLS
- Thread-local storage
- dlopen/dlsym
- Versioned symbols
- PIE executables (optional)
- ASLR (can be layered later)

## Dependencies

Required components:
- mmap() and VMR system (Prototype 25)
- ELF loader (Prototype 8/9)
- fork/execve model (Prototype 18/19)
- Per-process address spaces (Prototype 16)
- Page fault handling
- VFS file loading
- Userland libc (Prototype 14)

## Architecture Overview

Dynamic linking consists of:

1) Kernel loads main program ELF
2) Detects ET_DYN or PT_INTERP
3) Loads runtime loader (ld.so)
4) Transfers control to loader
5) Loader maps shared libraries
6) Performs relocations
7) Jumps to program entry point

Prototype 26 supports:

- One runtime loader
- One shared libc
- Flat dependency tree (no nested dependencies)

## ELF Extensions

### Supported ELF Types

- ET_EXEC (existing)
- ET_DYN (new)

ET_DYN programs:

- Must be relocatable
- Loaded at fixed base (no ASLR initially)

### Supported Program Headers

- PT_LOAD
- PT_DYNAMIC
- PT_INTERP (optional)

Ignore unsupported segments safely.

## Runtime Loader (ld.so)

### Location

Install loader as:

```

/LIB/ld.so

```

Kernel loads this automatically when PT_INTERP present.

### Responsibilities

ld.so must:

- Parse main executable dynamic section
- Locate required shared libraries
- Map libraries via mmap()
- Resolve symbols
- Apply relocations
- Call constructors (optional stub)
- Jump to program entry

## Shared Library Format

Use standard ELF shared object (.so):

- Built with -fPIC
- Contains relocation records
- Exported symbol table

Minimum supported relocations (x86_64):

- R_X86_64_RELATIVE
- R_X86_64_GLOB_DAT
- R_X86_64_JUMP_SLOT (for PLT)

PLT lazy binding optional.

## Symbol Resolution Model

Prototype 26 simplified lookup order:

1) Main executable
2) libc.so
3) Other shared libraries (if added later)

First match wins.

No symbol versioning.

## Memory Mapping Strategy

Libraries mapped:

- Read-only code segments
- Writable data segments
- Shared mappings across processes

Use MAP_SHARED for text segments.

Writable segments may be MAP_PRIVATE initially.

## Kernel vs User Loader Split

Preferred approach:

Userland loader (recommended):

- Kernel only loads loader
- Loader handles all linking logic
- Kernel remains simpler

Kernel-assisted linking optional but discouraged.

## libc Migration

Current static libc becomes:

- libc.so shared library
- User programs dynamically link against libc

Benefits:

- Smaller binaries
- Shared memory usage
- Centralized libc updates

## execve Integration

execve changes:

If ELF has PT_INTERP:

- Load loader instead of program
- Pass original program path as argument to loader
- Loader then loads actual executable

If ELF is ET_DYN without interpreter:

- Load directly (advanced use)

## Validation Tests

### TEST1: Shared libc

Build two programs dynamically linked:

- hello_dyn
- test_dyn

Expected:

- Both use libc.so
- libc mapped once per process
- Correct output

### TEST2: Relocation Correctness

Program references:

- Global variable in libc
- printf()

Expected:

- Correct addresses
- No crash

### TEST3: fork + Shared Library

Program forks.

Expected:

- Shared text segments remain shared
- Data segments isolated

### TEST4: Multiple Processes

Launch many dynamically linked programs.

Expected:

- No duplicate mapping of code segments
- Stable execution

## Safety Requirements

- Validate ELF dynamic structures
- Prevent mapping executable pages writable
- Protect kernel address space
- Verify relocation bounds
- Avoid symbol resolution overflow

## Implementation Notes

### Suggested Files

New:

- user/ld/loader.c
- user/ld/elf_dyn.c
- include/elf_dyn.h

Modified:

- src/elf.c
- src/syscall.c
- src/task.c
- userland build system

### Loader Placement

Loader should:

- Be statically linked initially
- Later can self-host dynamic loading

### Relocation Order

Process:

1) Map segments
2) Apply RELATIVE relocations
3) Resolve symbols
4) Apply remaining relocations
5) Jump to entry

### Page Protections

After relocations:

- Mark code pages RX
- Data pages RW
- Prevent RWX mappings

## Acceptance Criteria

Prototype 26 is complete when:

- Dynamically linked programs execute
- libc.so is shared correctly
- Relocations resolve correctly
- execve handles PT_INTERP binaries
- System stable under repeated dynamic loads

## Next Prototype (Planned)

Prototype 27: Virtual Filesystem Mounts and Device Nodes

Goals:
- Multiple filesystem mounts
- /dev pseudo filesystem
- Device file abstraction
- Prepare for modular drivers
```
