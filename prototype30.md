```md
# Prototype 30: POSIX ABI Stabilization and musl libc Port Preparation

## Purpose

Stabilize the kernel ABI and implement the remaining POSIX-critical syscalls and behaviors required to port a real C standard library (musl). This prototype is the transition from a hobby syscall set to a production-style userspace ABI.

After this prototype:

- Syscall ABI is frozen
- musl libc can be built against your kernel
- busybox becomes a realistic next target
- Native compilation path becomes viable

## Scope

In scope:
- Syscall ABI stabilization
- brk/sbrk heap management
- stat/fstat/lstat support
- getpid/getppid/getuid/getgid
- nanosleep
- clock_gettime (basic)
- errno standardization
- Kernel ABI headers for userspace
- Path normalization fixes

Out of scope:
- Threads (pthreads)
- Futex
- Advanced signals (sigaction)
- Networking
- Time zones
- Locale

## Dependencies

Required components:
- Memory isolation (Prototype 16)
- mmap() implementation (Prototype 25)
- Permissions and credentials (Prototype 28)
- FD layer (Prototype 21)
- Process lifecycle (Prototype 15)
- Hardening layer (Prototype 29)

## ABI Stabilization

### Syscall Table Freeze

Create public header:

```

include/uapi/syscall.h

````

Example:

```c
#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_fork        4
#define SYS_execve      5
#define SYS_wait        6
#define SYS_exit        7
#define SYS_brk         8
#define SYS_mmap        9
#define SYS_munmap      10
#define SYS_getpid      11
#define SYS_getppid     12
#define SYS_nanosleep   13
#define SYS_clock_gettime 14
#define SYS_stat        15
#define SYS_fstat       16
#define SYS_lstat       17
````

Once frozen:

* Never renumber
* Only append new syscalls

## brk / sbrk Support

### Process Heap Model

Add per-process fields:

```c
uint64_t brk_base;
uint64_t brk_current;
```

Behavior:

* brk_base set after ELF load
* brk_current tracks heap end

SYS_brk(new_end):

* Validate address range
* Map/unmap pages
* Update brk_current
* Return new break

sbrk implemented in libc wrapper.

## stat / fstat / lstat

Implement POSIX-compatible struct:

```c
struct stat {
    uint64_t st_size;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};
```

Map from:

* FAT metadata
* devfs metadata
* in-memory VFS nodes

Support:

* Regular files
* Directories
* Character devices

## Time Syscalls

### nanosleep()

Implement:

* Convert timespec to ticks
* Block process
* Wake via timer interrupt

Accuracy:

* Millisecond resolution acceptable.

### clock_gettime()

Support:

* CLOCK_REALTIME
* CLOCK_MONOTONIC

Use PIT ticks + boot time offset.

## Process Identity Syscalls

Implement:

* getpid()
* getppid()
* getuid()
* getgid()

Return values from task credentials.

## errno Convention

Syscalls return:

* Negative errno values

libc maps:

```
-EPERM -> errno=EPERM, return -1
```

Kernel must standardize:

* EFAULT
* EINVAL
* ENOENT
* EACCES
* ENOMEM
* EBUSY

## User ABI Headers

Create:

```
include/uapi/
```

Expose:

* syscall numbers
* stat struct
* signal numbers
* mmap flags
* open flags
* errno values

Used by:

* musl build
* userland programs

## Path Handling Fixes

Normalize:

* "." and ".."
* Multiple slashes
* Trailing slashes

Required by libc and busybox.

## Validation Tests

### TEST1: brk Growth

User program:

* malloc large blocks
* Verify memory accessible

Expected:

* Heap grows
* No overlap with mmap

### TEST2: stat Accuracy

stat("/BIN/HELLO.ELF")

Expected:

* Correct file size
* Mode bits correct
* UID/GID correct

### TEST3: nanosleep Timing

Sleep 500ms.

Expected:

* Approximately correct delay
* No busy looping

### TEST4: ABI Compatibility

Compile simple musl test:

* Uses open/read/write/malloc/time

Expected:

* Runs without crashes

## Safety Requirements

* Prevent heap overlap with stack/mmap
* Prevent brk shrinking below base
* Validate stat pointers
* Prevent time overflow
* Prevent integer overflow in address arithmetic

## Implementation Notes

### Suggested Files

New:

* include/uapi/syscall.h
* include/uapi/stat.h
* include/uapi/time.h
* src/brk.c
* src/time.c

Modified:

* src/syscall.c
* src/task.c
* src/vfs.c
* src/mmap.c
* libc wrappers

### Kernel/User Header Separation

Kernel uses:

```
include/
```

Userspace uses:

```
include/uapi/
```

Do not mix.

## Acceptance Criteria

Prototype 30 is complete when:

* Syscall ABI is frozen and documented
* brk works reliably
* stat/fstat return valid metadata
* nanosleep and clock_gettime work
* musl can be configured for your OS target
* busybox cross-build succeeds (even if not yet runnable)

## Next Prototype (Planned)

Prototype 31: musl libc Port and Native Userspace Runtime

Goals:

* Build musl against kernel ABI
* Replace custom libc
* Run busybox dynamically linked
* Establish production-grade userland foundation

```
```
