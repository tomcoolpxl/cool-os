```md
# Prototype 34: Self-Hosting Base System and Native Userland Build Environment

## Purpose

Transition coolOS from “can compile programs” to a **self-hosting development system**. This prototype establishes a minimal but complete native build environment capable of rebuilding the OS userland components on-device.

After this prototype:

- Core tools can be built natively
- System libraries can be rebuilt locally
- OS becomes partially self-hosting
- Development no longer depends on host Linux for routine userland work

## Scope

In scope:
- Native build of core userland tools
- Filesystem hierarchy standardization
- Build environment setup
- Toolchain runtime stabilization
- Package-style installation layout
- Rebuilding musl and basic tools on-device

Out of scope:
- Full GCC stage2 self-bootstrap
- C++ toolchain
- Advanced package manager
- Cross-compilation removal entirely
- Debugger ports (gdb)

## Dependencies

Required components:
- Native compiler (Prototype 32)
- musl libc runtime (Prototype 31)
- Stable syscall ABI (Prototype 30)
- mmap/brk stability
- Pipes and redirection
- VFS with reliable write support
- Permissions and users (Prototype 28)

## Filesystem Layout Standardization

Adopt conventional UNIX-style layout:

```

/
BIN      -> user executables
LIB      -> shared libraries
INCLUDE  -> headers
ETC      -> config files
TMP      -> temporary build files
HOME     -> user home directories
DEV      -> device filesystem

```

Kernel must ensure:

- Directories exist at boot
- Permissions correct
- TMP writable by normal users

## Native Build Toolset (Minimal)

Target native tools:

### Required

- tcc (already present)
- busybox (static or dynamic)
- sh (busybox shell)
- cp
- rm
- mkdir
- cat
- echo

### Optional (Stretch)

- make (small version like bmake or simple make)
- sed (later)
- grep (later)

## musl Native Rebuild

Goal:

Rebuild musl libc using native compiler.

Procedure:

1) Extract musl source on coolOS
2) Configure for coolOS target
3) Compile using tcc (or gcc stage1 if available)
4) Install into /LIB and /INCLUDE

Validation:

- Replace old libc
- Re-run busybox
- Verify system stability

## Kernel Headers Export

Export kernel UAPI headers to:

```

/INCLUDE/sys

```

Include:

- syscall.h
- stat.h
- time.h
- signal.h
- mmap.h

Required for:

- Native compilation
- musl rebuild
- Third-party ports

## Build Workspace Support

Add:

- /TMP mounted as tmpfs (RAM-backed)
- Or ensure FAT filesystem write performance acceptable

Compiler temporary files must:

- Create
- Write
- Delete safely

## Environment Variables (Minimal)

Implement minimal environment support:

- PATH
- HOME
- TMPDIR

Shell passes env to execve.

Compiler tools expect PATH to work.

## Native Build Flow Target

Desired workflow:

```

cd /TMP
cp /SRC/hello.c .
tcc hello.c -o hello
./hello

```

And:

```

cd /SRC/musl
./configure
make
make install

```

(Using simplified build scripts or manual steps)

## System Library Path Resolution

Dynamic loader must support:

- /LIB default search path
- LD_LIBRARY_PATH (optional)

Static fallback still allowed.

## Validation Tests

### TEST1: Native Busybox Rebuild

Recompile busybox using tcc.

Expected:

- Build succeeds
- New busybox runs

### TEST2: libc Rebuild

Rebuild musl.

Expected:

- New libc installed
- System programs still function

### TEST3: Multiple Build Jobs

Run two parallel compile jobs.

Expected:

- Scheduler handles load
- No filesystem corruption

### TEST4: Long Build Stability

Compile medium-sized codebase (busybox or musl).

Expected:

- No kernel panic
- No memory leaks
- Stable performance

## Safety Requirements

- Prevent filesystem corruption under concurrent writes
- Prevent out-of-memory during builds
- Enforce permission model for /BIN and /LIB
- Avoid loader crashes when replacing libc

## Implementation Notes

### Suggested Kernel Improvements

- Increase default process stack size
- Improve mmap fragmentation handling
- Tune scheduler time slice for compile workloads
- Improve page cache behavior for file IO (optional)

### Userland Improvements

- Add simple tar or unzip tool (optional)
- Add line editor support (already partially present)

## Acceptance Criteria

Prototype 34 is complete when:

- Core tools compile natively
- musl can be rebuilt on-device
- busybox rebuilt and runs
- Filesystem hierarchy stable
- System usable as self-hosted dev environment
- Host Linux no longer required for everyday userland builds

## Next Prototype (Planned)

Prototype 35: Full Native GCC Bootstrap and Toolchain Completion

Goals:
- Build stage1 GCC on coolOS
- Replace tcc for most builds
- Build binutils or LLVM lld natively
- Achieve full self-hosting toolchain
- Enable large software ports
```
