```md
# Prototype 31: musl libc Port and Native Userspace Runtime

## Purpose

Replace the custom minimal libc with musl libc and establish a production-quality userspace runtime environment. This prototype enables standard C programs to compile and run unmodified and forms the foundation required for native compilation and serious software ports.

After this prototype:

- musl runs natively on your OS
- Standard libc APIs become available
- busybox and real utilities can execute
- The OS transitions from experimental userspace to usable system

## Scope

In scope:
- musl libc port
- ABI glue layer implementation
- Syscall wrapper integration
- Thread stubs (single-threaded mode)
- Dynamic linking support (if enabled)
- Replace custom libc in user programs
- Busybox proof-of-run

Out of scope:
- pthreads and threading runtime
- Advanced locale and iconv
- Networking APIs
- C++ standard library
- Floating-point heavy math optimizations

## Dependencies

Required components:
- POSIX ABI stabilization (Prototype 30)
- execve + fork + mmap + brk
- ELF dynamic loader (Prototype 26)
- File descriptor layer (Prototype 21)
- Permissions model (Prototype 28)
- Signals (Prototype 23)
- TTY subsystem (Prototype 24)

## musl Port Strategy

### Target Configuration

Create new musl target:

Example:

```

arch: x86_64
os: coolos
libc: musl

```

Provide:

- syscalls mapping
- errno mapping
- time syscalls mapping
- signal stubs

## Syscall Glue Layer

Implement musl syscall backend:

File:

```

arch/x86_64/syscall_arch.h

```

Map musl syscalls to your kernel ABI.

Example:

```

__syscall(SYS_write, fd, buf, len)

```

Use SYSCALL instruction.

## Required Syscalls for musl

Minimum set:

Process:

- fork
- execve
- waitpid
- exit
- getpid

Memory:

- brk
- mmap
- munmap

File:

- open
- close
- read
- write
- stat
- fstat
- lseek

Time:

- nanosleep
- clock_gettime

Signals:

- sigaction (stub minimal)
- kill

TTY:

- ioctl (stub returning ENOTTY initially)

## Build System Integration

Cross-compile musl:

On Linux host:

```

./configure --target=x86_64-coolos --prefix=/usr
make

```

Install output into:

- /LIB
- /BIN

Generate:

- libc.so
- dynamic loader
- headers

## libc Deployment Model

Two options:

### Option A: Dynamic libc (Recommended)

- Programs link dynamically to libc.so
- Kernel loader loads ld.so
- Smaller binaries
- Shared memory savings

### Option B: Static libc

- Easier initial bring-up
- Larger binaries
- Fewer loader dependencies

Prototype 31 supports both.

## Busybox Validation Target

Build busybox using musl toolchain.

Target:

- Static busybox first
- Later dynamic busybox

Busybox features required:

- ls
- cat
- echo
- sh (optional)

Goal:

```

busybox ls /

```

works on your OS.

## Kernel Changes Required

Fix uncovered issues discovered by musl:

Common fixes required:

- Proper errno propagation
- Correct signal behavior
- Accurate stat flags
- Correct file descriptor inheritance
- mmap alignment correctness
- brk overlap handling

Expect iteration.

## Validation Tests

### TEST1: libc Hello World

Compile:

```

#include <stdio.h>
int main() {
printf("hello musl\n");
}

```

Expected:

- Output prints correctly
- No crash

### TEST2: Busybox ls

Run:

```

busybox ls /

```

Expected:

- Directory listing appears

### TEST3: fork Stress

Busybox shell:

```

for i in 1 2 3 4; do echo test; done

```

Expected:

- Correct output
- No crash

### TEST4: File IO

Busybox:

```

cat /BIN/HELLO.ELF

```

Expected:

- Correct content printed

## Safety Requirements

- musl must not panic kernel
- No memory corruption under heavy libc use
- Proper syscall error mapping
- Stable dynamic loader behavior

## Implementation Notes

### Suggested Files

Userland:

- toolchain config
- musl patches
- loader config

Kernel:

- syscall glue fixes
- stat compatibility fixes
- signal compatibility fixes

### Debugging Strategy

Enable:

- strace-like syscall logging (optional kernel debug mode)
- Verbose loader logs
- Assertion checks

## Acceptance Criteria

Prototype 31 is complete when:

- musl builds successfully
- Programs link and run against musl
- busybox executes at least basic commands
- System remains stable under multi-process workloads
- Custom libc can be removed or deprecated

## Next Prototype (Planned)

Prototype 32: Native Toolchain Bootstrap (tcc and GCC Stage1)

Goals:
- Run tinycc natively
- Compile simple programs on-device
- Begin self-hosting toolchain
- Prepare for full GCC bootstrap
```
