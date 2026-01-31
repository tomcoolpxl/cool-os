# Prototype 14: Userland stdio and Minimal libc Expansion

## Purpose

Provide a usable C runtime layer for user programs by expanding the minimal libc and introducing a basic stdio abstraction. This prototype allows user programs to perform formatted output, buffered writes, and simple input without directly invoking raw syscalls.

This is a prerequisite for porting real software (including DOOM) and writing non-trivial user programs.

## Scope

In scope:
- Userland libc expansion
- Syscall wrappers consolidation
- stdout abstraction
- Minimal printf implementation
- Basic memory and string utilities
- Optional stdin support via blocking read syscall (if available)

Out of scope:
- Full POSIX compliance
- FILE* streams
- Dynamic linking
- Locale, wide characters
- Floating-point printf formatting
- Signal handling

## Dependencies

Required components:
- User mode + syscall interface (Prototype 7)
- ELF loader and user program execution (Prototype 8/9)
- Framebuffer console output path (Prototype 11)
- Kernel shell (Prototype 13)
- SYS_write and SYS_exit syscalls
- Cooperative multitasking

Recommended:
- SYS_read syscall for keyboard input (if implemented)
- User heap allocator (simple sbrk-style or static heap)

## Architecture Overview

User programs will link against a custom static libc providing:

- Program entry (_start)
- Syscall wrappers
- Basic libc functions
- stdio-like helpers (printf, puts)

Output path:

```

user printf -> libc formatter -> SYS_write -> kernel -> console -> framebuffer

````

## Startup Runtime (crt0)

Provide a user entry stub:

File: user/crt0.S

Responsibilities:
- Set up stack alignment
- Call main(int argc, char **argv)
- Call exit() with return value

For Prototype 14:

- argc = 0
- argv = NULL

Later prototypes may extend this.

## Syscall Wrapper Layer

Centralize syscall entry in libc:

Provide:

```c
long syscall(long num,
             long a1,
             long a2,
             long a3,
             long a4,
             long a5);
````

Implement wrappers:

```c
void exit(int code);
ssize_t write(int fd, const void *buf, size_t len);
void yield(void);
```

File descriptors:

* 1 = stdout
* 2 = stderr (optional, same as stdout)

## stdout Abstraction

Provide minimal helpers:

```c
int putchar(int c);
int puts(const char *s);
```

Behavior:

* Use write(1, ...)
* Append newline in puts()

## printf Implementation

Provide:

```c
int printf(const char *fmt, ...);
```

Supported format specifiers (minimum):

* %s (string)
* %d (signed decimal)
* %u (unsigned decimal)
* %x (hexadecimal)
* %c (character)
* %% (literal percent)

Unsupported:

* floating point
* width/precision modifiers
* flags

Implementation strategy:

* Parse format string
* Use small integer-to-string helpers
* Write into temporary buffer
* Flush via write()

## String and Memory Utilities

Provide basic libc functions:

Mandatory:

```c
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int val, size_t n);
```

Optional:

```c
char *strncpy(...)
```

## User Heap (Optional but Recommended)

Provide minimal allocator:

Approach A (simplest):

* Static fixed-size heap buffer
* Bump allocator

Approach B:

* brk/sbrk syscall (future prototype)

For Prototype 14:

* Static heap of 64–256 KB inside libc is sufficient.

Provide:

```c
void *malloc(size_t size);
void free(void *ptr); // optional stub
```

Free may be a no-op initially.

## Integration With Build System

User program build must:

* Link against libc static archive
* Use custom linker script
* Use crt0 entry point

Result:

* Produces static ELF64 binaries runnable by your loader.

## Validation Tests

### TEST1: printf formatting

User program:

```c
printf("Hello %s %d %x\n", "world", 42, 42);
```

Expected output:

```
Hello world 42 2a
```

### TEST2: puts and putchar

User program prints characters and lines.

Expected:

* Correct ordering
* No missing output

### TEST3: Multiple user programs

Launch several programs using shell:

* Each prints formatted output
* Yield between prints

Expected:

* Correct interleaving
* No memory corruption

### TEST4: Stress output

User program prints 1000 lines.

Expected:

* No kernel crash
* Console scroll works
* No deadlock

## Safety Requirements

* printf buffer must be bounded
* No stack overflow from formatting
* All user pointers passed to write must be validated in kernel
* No kernel panic on malformed format strings

## Deliverables

New files:

* user/crt0.S
* libc source files (printf.c, string.c, syscall.c, malloc.c)

Modified:

* Makefile (libc build + link)
* User program build rules

## Acceptance Criteria

Prototype 14 is complete when:

* User programs link against libc
* printf works reliably
* User programs no longer need raw syscall wrappers
* Shell-launched programs use libc output
* System remains stable under heavy output
