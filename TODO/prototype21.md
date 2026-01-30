````md
# Prototype 21: File Descriptor Table and Userland I/O Expansion

## Purpose

Introduce a real per-process file descriptor (FD) table and unify all userland I/O (stdin, stdout, stderr, files) behind a consistent interface. This enables redirection, pipes (future), and proper stream handling.

After this prototype:

- Every process has its own FD table
- SYS_read and SYS_write operate on descriptors
- Shell and programs can use standard I/O semantics
- The OS transitions from ad-hoc console/file access to a structured I/O model

## Scope

In scope:
- Per-process FD table
- stdin/stdout/stderr setup
- SYS_read and SYS_write routed through FD layer
- File-backed descriptors (FAT/VFS)
- Console-backed descriptors
- Close syscall
- Duplication groundwork (dup semantics optional stub)

Out of scope:
- Pipes
- Redirection operators in shell
- Select/poll
- Non-blocking IO
- Sockets
- Permissions model

## Dependencies

Required components:
- Userland shell (Prototype 20)
- execve + fork (Prototypes 18, 19)
- VFS file access (Prototype 9)
- Keyboard input system (Prototype 12)
- Framebuffer console (Prototype 11)
- Process lifecycle (Prototype 15)
- Scheduler and preemption (Prototype 17)

## File Descriptor Model

### Descriptor Numbers

Standard conventions:

- 0 = stdin
- 1 = stdout
- 2 = stderr

All other descriptors allocated sequentially.

### FD Table Structure

Extend process structure:

```c
#define MAX_FD 32

typedef struct file_desc {
    int used;
    int type;        // FD_CONSOLE, FD_FILE, FD_KBD, etc
    void *handle;    // FS handle or device pointer
    size_t offset;   // File position (for files)
    int flags;
} file_desc_t;
````

Add:

* file_desc_t fd_table[MAX_FD];

Each process owns its own table.

## Descriptor Types

Minimum supported types:

### FD_CONSOLE_OUT

Maps to:

* framebuffer console output
* serial mirror (optional)

Used by stdout and stderr.

### FD_CONSOLE_IN / FD_KBD

Maps to:

* keyboard input buffer

Used by stdin.

### FD_FILE

Maps to:

* FAT/VFS file handle

Used for disk files.

## Initialization Rules

### Kernel Init Process

When INIT process is created:

Initialize:

* fd 0 -> keyboard input
* fd 1 -> console output
* fd 2 -> console output

These are inherited by forked children.

## Syscall Changes

### SYS_write

Old behavior:

* Direct console write

New behavior:

```
ssize_t write(int fd, const void *buf, size_t len)
```

Kernel:

* Validate fd
* Dispatch based on descriptor type:

FD_CONSOLE_OUT:

* Print to console

FD_FILE:

* Write unsupported for now (return error)

Return:

* Bytes written
* -1 on error

### SYS_read

Implement:

```
ssize_t read(int fd, void *buf, size_t len)
```

Behavior:

FD_KBD:

* Blocking read from keyboard buffer
* Fill user buffer

FD_FILE:

* Read from FAT/VFS handle
* Update offset

FD_CONSOLE_IN:

* Alias to keyboard input

Return:

* Bytes read
* 0 on EOF
* -1 on error

### SYS_close

Implement:

```
int close(int fd)
```

Behavior:

* Validate fd
* Free associated handle if FD_FILE
* Mark entry unused

fd 0,1,2 may not be closable initially (optional restriction).

## Fork Behavior

When fork() is called:

* Child receives copy of FD table
* File handles are shared
* File offsets are shared (POSIX-like behavior)

Reference counting may be required for file handles.

## execve Behavior

When execve() is called:

* FD table is preserved
* stdin/stdout/stderr remain intact
* All other open descriptors remain open

Later prototypes may add FD_CLOEXEC flags.

## Shell Integration

Modify shell:

Instead of using direct console functions:

* Use libc read/write APIs
* Shell input comes from fd 0
* Shell output goes to fd 1

This tests FD correctness.

## Validation Tests

### TEST1: stdin/stdout Echo

Shell:

```
type
```

(type is a simple user program)

Program:

* Reads from stdin
* Writes back to stdout

Expected:

* Typed characters echo back
* Enter terminates

### TEST2: File Read via FD

User program:

* open("TEST.TXT")
* read(fd, buf)
* write(1, buf)

Expected:

* File contents printed to screen

### TEST3: Fork FD Sharing

Program:

* Parent opens file
* fork()
* Child reads
* Parent reads

Expected:

* Shared offset behavior visible
* No crash

### TEST4: Close Handling

Program:

* open file
* close fd
* attempt read

Expected:

* Error returned
* Kernel remains stable

## Safety Requirements

* Validate fd index bounds
* Validate user buffer pointers
* Prevent use-after-close
* Prevent kernel pointer exposure
* Protect FD table access with short critical sections

## Implementation Notes

### Suggested Files

New:

* include/fd.h
* src/fd.c

Modified:

* src/syscall.c
* src/task.c
* src/vfs.c
* src/kernel.c
* libc read/write wrappers

### Error Codes

Use simple conventions:

* -1 for error
* 0 for EOF
* Positive values for success

POSIX errno not required yet.

### Blocking Reads

stdin read should:

* Sleep using hlt
* Wake on keyboard interrupt
* Not busy wait

## Acceptance Criteria

Prototype 21 is complete when:

* Every process has its own FD table
* stdin/stdout/stderr work via syscalls
* File reads use FD abstraction
* Shell uses FD-based IO exclusively
* fork and exec preserve descriptors correctly
* System remains stable under repeated IO operations

## Next Prototype (Planned)

Prototype 22: Pipes and Shell Redirection

Goals:

* Implement pipe()
* Kernel ring buffer for pipe endpoints
* Support "|" and ">" in userland shell
* Enable program composition

```
```
