```md
# Prototype 35: Full Native GCC Bootstrap and Complete Self-Hosting Toolchain

## Purpose

Complete the transition to a fully self-hosting operating system by bootstrapping GCC natively and establishing a standard compiler + linker toolchain running entirely on coolOS.

After this prototype:

- GCC runs natively
- The OS can compile itself (userland components)
- tcc becomes optional
- Large third-party software becomes realistically portable
- coolOS reaches "serious OS" maturity milestone

## Scope

In scope:
- Native GCC stage1 bootstrap
- libgcc runtime support
- Native linker availability (binutils ld or LLVM lld)
- Compiler driver integration
- C toolchain path standardization
- System rebuild using native compiler

Out of scope:
- C++ standard library (libstdc++)
- Rust toolchain
- Debugger ports (gdb)
- Cross-compilation removal entirely
- Optimizing compiler performance

## Dependencies

Required components:
- Native tcc bootstrap (Prototype 32)
- musl libc running natively (Prototype 31)
- Stable filesystem and TMP workspace (Prototype 34)
- mmap/brk stability
- fork/exec heavy workload stability
- Pipes and redirection
- Dynamic loader stability

## Bootstrap Strategy

### Phase 1: Cross-Seeded GCC Binary

Initial GCC binary is introduced by:

- Cross-compiling GCC stage1 on host Linux
- Copying gcc binary + libgcc to coolOS

This avoids chicken-and-egg problem.

### Phase 2: Native GCC Rebuild

Using seeded gcc:

- Rebuild GCC on coolOS
- Rebuild libgcc
- Replace seeded binary

After success:

- Toolchain becomes native-generated

## Filesystem Layout

Standardize toolchain locations:

```

/BIN
gcc
ld
as
/LIB
libc.so
libgcc.so
crt1.o
crti.o
crtn.o
/INCLUDE
stdio.h
stdlib.h
sys/

```

Dynamic loader must search /LIB by default.

## Required Toolchain Components

### GCC Components

Minimum:

- gcc driver
- cc1 (C frontend)
- libgcc runtime

C++ frontend not required.

### Linker Options

Choose one:

Option A (Simplest):
- Port binutils ld

Option B (Recommended long-term):
- Port LLVM lld (smaller, faster, cleaner codebase)

Prototype 35 requires at least one working ELF linker.

### Assembler

If GCC emits assembly:

- as required

Alternative:

- Configure GCC to emit object code directly using integrated assembler if supported.

## Runtime Support Objects

You must provide:

- crt1.o
- crti.o
- crtn.o

These contain:

- Program entry
- Stack setup
- libc startup glue

musl already provides compatible startup objects.

Verify loader compatibility.

## Kernel Requirements Validation

Before GCC bootstrap ensure:

- mmap supports large allocations
- file IO stable under heavy usage
- stat/fstat correct
- PATH resolution correct
- signals not interfering with compiler

Compiler workloads stress kernel heavily.

## Native Build Flow Target

Goal workflow:

```

gcc hello.c -o hello
./hello

```

And:

```

gcc busybox.c -o busybox

```

Using native GCC only.

## Self-Hosting Validation

### Stage1 Validation

Run:

```

gcc --version

```

Expected:
- Reports correct GCC version

### Stage2 Validation

Rebuild GCC using native GCC:

```

./configure
make

```

Expected:
- Compilation completes
- Produces identical or compatible compiler

### System Rebuild Test

Rebuild:

- busybox
- musl libc

Using only native GCC.

Expected:

- Successful builds
- No host artifacts required

## Performance Expectations

Initial GCC build will be slow.

Acceptable:

- Minutes per file
- Hours for full build on emulator

Stability more important than speed.

## Safety Requirements

- Prevent OOM killing kernel
- Graceful handling of memory pressure
- Prevent filesystem corruption
- Ensure compiler crashes do not kill OS

Add:

- Memory usage limits per process (optional)
- tmp cleanup on reboot

## Implementation Notes

### Suggested Kernel Improvements

- Increase default user stack to 8MB+
- Improve virtual memory fragmentation handling
- Add swap file support (optional stretch)
- Improve scheduler fairness for long CPU tasks

### Userland Improvements

- Add make tool
- Add tar utility
- Add basic build scripts

## Acceptance Criteria

Prototype 35 is complete when:

- gcc runs natively
- gcc can compile simple programs
- gcc can rebuild itself (stage2)
- musl can be rebuilt with gcc
- busybox can be rebuilt with gcc
- System no longer depends on host compiler for core tools

## Final Milestone Status

After Prototype 35:

coolOS becomes:

- Self-hosting
- Capable of native software development
- Able to port medium-scale Linux software
- Suitable for serious experimentation

## Next Long-Term Direction (Optional)

Possible future tracks:

- Networking stack + curl/ssh
- Graphical window system
- Package manager
- SMP and multicore scheduling
- Filesystem journaling
- USB support
- Rust toolchain bootstrap
```
