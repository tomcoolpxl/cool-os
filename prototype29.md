````md
# Prototype 29: Syscall Hardening and Capability-Based Privilege Isolation

## Purpose

Strengthen kernel security by hardening the syscall interface and introducing a capability-based privilege model. This prototype reduces the kernel attack surface, enforces fine-grained access control, and prepares the system for multi-user and network-facing workloads.

After this prototype:

- All syscalls are validated rigorously
- Dangerous operations are gated by explicit capabilities
- Kernel boundary becomes robust against malformed input
- Privilege escalation paths are closed

## Scope

In scope:
- Syscall argument validation framework
- Safe user pointer access helpers
- Capability flags per process
- Privilege checks on sensitive syscalls
- Kernel panic reduction
- Security auditing hooks

Out of scope:
- Mandatory access control (SELinux-like)
- Sandboxing namespaces
- seccomp filters
- Hardware virtualization
- Kernel address space layout randomization (KASLR)

## Dependencies

Required components:
- Syscall infrastructure
- File descriptor layer (Prototype 21)
- Permissions and users (Prototype 28)
- Process lifecycle (Prototype 15)
- Memory isolation (Prototype 16)
- mmap subsystem (Prototype 25)

## Capability Model

### Capability Flags

Introduce capability bitmask:

```c
typedef uint64_t cap_t;

#define CAP_MOUNT        (1ULL << 0)
#define CAP_DEV_CREATE  (1ULL << 1)
#define CAP_RAW_IO      (1ULL << 2)
#define CAP_SIGNAL_ALL  (1ULL << 3)
#define CAP_SYS_ADMIN   (1ULL << 4)
````

Each process has:

```c
cap_t caps;
```

Root process starts with:

* All capabilities enabled

Normal users:

* Zero or minimal capabilities

## Syscall Classification

Split syscalls into:

### Unprivileged

No capability required:

* read
* write
* close
* fork
* execve
* wait
* exit
* yield
* mmap anonymous (read/write only)

### Privileged

Require capability:

Mount:

* mount() -> CAP_MOUNT

Device management:

* mknod() -> CAP_DEV_CREATE

Signal control:

* kill(other pid) -> CAP_SIGNAL_ALL

Raw IO:

* port IO access -> CAP_RAW_IO

System administration:

* reboot -> CAP_SYS_ADMIN

## User Pointer Validation

### Safe Copy Helpers

Implement:

```c
int copy_from_user(void *dst, const void *user_src, size_t len);
int copy_to_user(void *user_dst, const void *src, size_t len);
```

Behavior:

* Validate user address range
* Catch page faults
* Return error instead of kernel panic

### String Helpers

Implement:

```c
int copy_string_from_user(char *dst, const char *user_src, size_t max);
```

Used for:

* execve path
* open path
* ioctl names

## Kernel Memory Access Rules

Kernel must never:

* Directly dereference user pointers
* Trust size arguments
* Trust structure layout from user memory

All access must go through validation helpers.

## Syscall Return Semantics

Standardize error returns:

* Return negative error codes:

Examples:

* -EINVAL
* -EFAULT
* -EPERM
* -ENOENT

libc maps to errno (optional later).

## Attack Surface Reduction

### Null Pointer Guard

Disallow mapping of page zero in user space:

* Prevent NULL dereference exploits

### Kernel Address Range Guard

Reject user pointers:

* > = kernel virtual base
* Outside canonical user address range

## Signal Permission Checks

Only allow:

* Sending signal to own processes
* Or if CAP_SIGNAL_ALL present

Shell running as root has capability.

## Execve Hardening

Validate:

* ELF headers
* Segment sizes
* Entry point range
* Overlapping segments
* Stack setup correctness

Reject malformed binaries gracefully.

## FD Layer Hardening

Validate:

* FD index bounds
* Access mode (read/write)
* Type compatibility

Prevent:

* Writing to read-only FD
* Reading from write-only FD

## Validation Tests

### TEST1: Invalid Pointer Syscall

User program:

* Pass invalid pointer to write()

Expected:

* write returns error
* Kernel survives

### TEST2: Privileged Operation Denial

Normal user attempts:

* mount()
* kill other process

Expected:

* Permission denied

### TEST3: Capability Grant

Run as root:

* Perform privileged operation

Expected:

* Allowed

### TEST4: Fuzz Test

User program:

* Random syscall arguments
* Large lengths
* Invalid fds

Expected:

* No kernel crash
* Errors returned correctly

## Safety Requirements

* No kernel panic from user input
* All user pointers validated
* No privilege escalation
* No memory corruption
* Deterministic error handling

## Implementation Notes

### Suggested Files

New:

* include/uaccess.h
* src/uaccess.c
* include/capability.h
* src/capability.c

Modified:

* src/syscall.c
* src/task.c
* src/vfs.c
* src/mmap.c
* src/signal.c

### Incremental Hardening

Do not convert all syscalls at once.

Approach:

1. Harden write/read
2. Harden execve
3. Harden mmap
4. Harden signals

Test after each step.

## Acceptance Criteria

Prototype 29 is complete when:

* All syscalls validate arguments
* Capability checks enforced
* Invalid user input no longer crashes kernel
* Root vs user privilege separation works
* System survives fuzz-like abuse

## Next Prototype (Planned)

Prototype 30: Networking Stack (Loopback + Basic TCP/IP)

Goals:

* Network device abstraction
* Loopback interface
* IP stack
* Basic TCP sockets
* Prepare for remote access and networking tools

```
```
