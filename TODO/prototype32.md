```md
# Prototype 32: Native Toolchain Bootstrap (tcc First, GCC Stage1 Preparation)

## Purpose

Introduce native compilation capability by bootstrapping a C compiler that runs directly on your OS. This prototype focuses on bringing up TinyCC (tcc) first, then preparing the environment for a future GCC stage1 bootstrap.

After this prototype:

- You can compile C programs natively on coolOS
- The system becomes self-hosting for simple software
- Userland development no longer depends entirely on cross-compilation
- The OS reaches a major milestone of autonomy

## Scope

In scope:
- Port and run TinyCC (tcc) natively
- Native compilation of simple C programs
- Toolchain runtime support (linker, assembler stubs)
- File system layout for toolchain
- Dynamic/static linking with musl
- Build environment bootstrap

Out of scope:
- Full GCC native bootstrap
- C++ compiler
- make/ninja ports
- Advanced linker scripts
- Debuggers

## Dependencies

Required components:
- musl libc running natively (Prototype 31)
- Dynamic loader or static linking support
- mmap/brk stability
- File IO and directory creation
- Process model stability
- TTY shell environment
- VFS with write support

## Toolchain Strategy

### Phase Order

Correct bootstrap sequence:

1) Cross-build tcc on Linux host
2) Transfer tcc binary and runtime files to coolOS
3) Run tcc natively
4) Compile test programs locally
5) Build stage1 GCC later (Prototype 33+)

Do NOT attempt GCC first.

## Filesystem Layout

Standardize toolchain paths:

```

/BIN
tcc
/LIB
libc.so
libtcc1.a
/INCLUDE
stdio.h
stdlib.h
...
/TMP
/HOME

```

Add /TMP tmpfs or RAM-backed directory if disk IO is slow.

## TinyCC Port Requirements

### Build Configuration

Cross-build tcc with:

- Target: x86_64-coolos
- Output: static or musl-dynamic binary
- Disable optional features:
  - libdl
  - threads
  - networking

### Runtime Requirements

Kernel must support:

- fork/exec
- open/read/write/close
- mmap
- brk
- fstat
- pipe (for compiler internal pipes)
- environment variables optional (can stub)

## Linking Model

### Stage 1: Static Linking

Simplest approach:

- Compile output as static ELF
- Use musl static libc

Advantages:

- Avoid dynamic loader complexity
- Easier debugging

### Stage 2: Dynamic Linking (Optional)

Later:

- Produce dynamically linked binaries
- Use libc.so

## Assembler and Linker

### Approach A (Recommended)

Use tcc internal assembler and linker.

Advantages:

- No binutils port needed initially
- Simplifies bootstrap

### Approach B (Later)

Port:

- ld (binutils)
- as

Not required for Prototype 32.

## Native Compile Workflow

Goal workflow:

```

tcc hello.c -o hello
./hello

```

Expected:

- Compiles without crash
- Program runs successfully

## Kernel Adjustments Likely Needed

During bring-up expect to fix:

- fstat correctness
- lseek behavior
- PATH resolution
- file permission flags
- memory mapping edge cases
- stack size limits
- argument passing correctness

These must be stabilized.

## Validation Tests

### TEST1: Hello World Native Build

On coolOS:

```

echo '#include <stdio.h>
int main(){printf("native ok\n");}' > hello.c

tcc hello.c -o hello
./hello

```

Expected:

```

native ok

```

### TEST2: Compile Loop Stress

Compile same file 10 times.

Expected:

- No memory leak
- No kernel crash

### TEST3: File IO During Compile

Verify:

- temp files created
- object files generated
- no corruption

### TEST4: fork Stress During Compile

Run two parallel tcc processes.

Expected:

- Both compile successfully
- Scheduler handles load

## Shell Integration

Shell should support:

- Executing compiler
- Redirecting output:

```

tcc hello.c -o hello > build.log

```

Ensure pipes and redirection work reliably.

## Safety Requirements

- Compiler must not crash kernel
- Invalid C input must not panic OS
- Memory pressure handled gracefully
- Temporary file cleanup does not leak descriptors

## Implementation Notes

### Suggested Work Items

Kernel:

- Improve fstat/stat correctness
- Improve file write reliability
- Increase default user stack size
- Verify mmap behavior under load
- Add exec argument vector support if incomplete

Userland:

- Cross-build tcc
- Install musl headers
- Provide minimal include tree

### Debugging Tools

Add optional kernel debug flag:

- Log syscalls from tcc
- Trace mmap/brk usage

Very useful during bootstrap.

## Acceptance Criteria

Prototype 32 is complete when:

- tcc runs natively
- hello world compiles and executes locally
- Multiple native builds work
- Shell usable during compilation
- System stable under compile workloads

## Next Prototype (Planned)

Prototype 33: Native Build Utilities and Stage1 GCC Bootstrap

Goals:
- Port make
- Port binutils (ld/as) or LLVM lld
- Bootstrap minimal GCC
- Build libc natively
- Move toward fully self-hosting OS
```
