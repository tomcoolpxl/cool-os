````md
# Prototype 22: Pipes and Shell Redirection

## Purpose

Introduce inter-process communication via anonymous pipes and enable shell redirection and pipelines. This allows programs to be chained together using standard UNIX-style semantics and completes the core I/O model.

After this prototype:

- pipe() exists
- read/write work on pipe endpoints
- Shell supports "|" and ">" operators
- Programs can communicate without temporary files

## Scope

In scope:
- pipe() syscall
- Kernel pipe buffer implementation
- Blocking semantics for pipes
- FD table integration
- Shell support for pipelines
- Output redirection to files (write-only)
- Basic error handling

Out of scope:
- Named pipes (FIFOs)
- Non-blocking IO
- select/poll/epoll
- Signals (SIGPIPE)
- Background pipelines
- Advanced redirection syntax (2>, <, etc)

## Dependencies

Required components:
- FD table and IO layer (Prototype 21)
- fork() and execve() (Prototypes 18, 19)
- Userland shell (Prototype 20)
- Scheduler and preemption (Prototype 17)
- VFS file open/read support
- Kernel heap

## Pipe Model

### pipe() API

Userland:

```c
int pipe(int fd[2]);
````

Kernel behavior:

* Allocate pipe object
* Create two file descriptors:

  * fd[0] = read end
  * fd[1] = write end
* Return 0 on success, -1 on error

## Kernel Pipe Structure

Example:

```c
#define PIPE_BUF_SIZE 4096

typedef struct pipe {
    uint8_t buffer[PIPE_BUF_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t count;

    int readers;
    int writers;
} pipe_t;
```

Store pointer to pipe_t in FD handle.

## Pipe Semantics

### Write Behavior

SYS_write on pipe:

* If buffer has space:

  * Copy data
  * Advance write_pos
  * Increase count
* If buffer full:

  * Block process until space available

### Read Behavior

SYS_read on pipe:

* If buffer has data:

  * Copy data
  * Advance read_pos
  * Decrease count
* If buffer empty:

  * Block process until data available

### EOF Handling

If:

* All writers closed
* Buffer empty

Then:

* read() returns 0 (EOF)

### Close Semantics

SYS_close on pipe FD:

* Decrement readers or writers
* If both reach zero:

  * Free pipe object

## Scheduler Integration

Blocked pipe readers/writers:

* Must set process state to PROC_BLOCKED
* Scheduler must wake blocked processes when:

  * Writer adds data
  * Reader consumes data
  * Endpoint closes

Avoid busy waiting.

## File Redirection Support

### Output Redirection (>)

Shell syntax:

```
command > file.txt
```

Behavior:

Shell:

1. open("file.txt")
2. fork()
3. Child:

   * dup fd to stdout (fd 1)
   * close unused fds
   * execve(command)
4. Parent:

   * close file fd
   * wait()

Kernel requirements:

* Support opening file in write mode (truncate or create)
* FAT write support limited to sequential overwrite is acceptable for this prototype

If FAT write is not yet implemented:

* Implement redirection to RAM-backed pseudo-file
* Or limit redirection to overwrite-only preallocated file

## Pipeline Support (|)

Shell syntax:

```
cmd1 | cmd2
```

Shell behavior:

1. pipe(fd)
2. fork first child:

   * dup fd[1] to stdout
   * close fd[0]
   * execve(cmd1)
3. fork second child:

   * dup fd[0] to stdin
   * close fd[1]
   * execve(cmd2)
4. Parent:

   * close both ends
   * wait for both children

Support only single pipe in Prototype 22.

Chained pipelines optional:

```
a | b | c
```

Not required.

## dup() Support (Minimal)

Implement minimal:

```c
int dup(int oldfd);
```

Behavior:

* Find free FD slot
* Point to same underlying handle
* Increment reference count

dup2 optional.

## FD Table Reference Counting

Extend FD entries with:

* refcount or shared handle pointer

Ensure:

* Closing duplicated FD decrements correctly
* Underlying object freed only when count reaches zero

## Validation Tests

### TEST1: Simple Pipe

User programs:

Producer:

* writes "hello\n" to stdout

Consumer:

* reads stdin and prints to stdout

Shell:

```
producer | consumer
```

Expected:

```
hello
```

### TEST2: Blocking Behavior

Producer:

* Writes large data continuously

Consumer:

* Reads slowly

Expected:

* Producer blocks when pipe full
* No kernel deadlock

### TEST3: Redirection

Shell:

```
hello > out.txt
cat out.txt
```

Expected:

* Output redirected correctly

### TEST4: Close Propagation

Producer exits immediately.

Consumer reads.

Expected:

* Consumer receives EOF
* Exits cleanly

## Safety Requirements

* Validate all FD indices
* Prevent kernel memory exposure
* Prevent pipe buffer overflow
* Avoid deadlocks in blocked state
* Always wake correct waiting processes

## Implementation Notes

### Suggested Files

New:

* include/pipe.h
* src/pipe.c

Modified:

* src/syscall.c
* src/fd.c
* src/scheduler.c
* userland shell parser

### Buffer Strategy

Use circular buffer:

* write_pos = (write_pos + 1) % PIPE_BUF_SIZE
* read_pos = (read_pos + 1) % PIPE_BUF_SIZE

Avoid memmove inside pipe.

### Wakeup Strategy

Keep simple wait queues:

* one for readers
* one for writers

Wake on state change.

## Acceptance Criteria

Prototype 22 is complete when:

* pipe() works reliably
* read/write blocking behavior correct
* Shell "|" operator works
* Output redirection works
* No deadlocks or leaks
* System remains stable under repeated pipe usage

## Next Prototype (Planned)

Prototype 23: Signal Delivery and Basic Job Control

Goals:

* SIGINT (Ctrl+C)
* Process termination from terminal
* Background task control groundwork
* Shell interrupt handling

```
```
