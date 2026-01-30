````md
# Prototype 24: Terminal Line Discipline and Expanded Job Control

## Purpose

Introduce a real terminal (TTY) line discipline and expand job control functionality. This prototype separates raw keyboard input from cooked terminal input, enables interactive shell behavior, and adds background job support.

After this prototype:

- Input is line-buffered and editable by the terminal layer
- Foreground/background jobs are supported
- SIGSTOP/SIGCONT job control works
- Shell behaves like a real interactive terminal

## Scope

In scope:
- TTY abstraction layer
- Line discipline (canonical input mode)
- Input editing (backspace, line kill, clear line)
- Echo control
- Foreground/background job management
- SIGSTOP and SIGCONT support
- Shell job control commands

Out of scope:
- Full POSIX termios interface
- Advanced escape sequence handling
- Arrow keys/history navigation
- Unicode/UTF-8 input
- Pseudoterminals (PTYs)

## Dependencies

Required components:
- Signals and Ctrl+C handling (Prototype 23)
- File descriptor layer (Prototype 21)
- Pipes and redirection (Prototype 22)
- Keyboard driver (Prototype 12)
- Framebuffer console (Prototype 11)
- Scheduler and preemption (Prototype 17)
- Userland shell (Prototype 20)

## TTY Architecture

### TTY Device Abstraction

Introduce a tty object:

```c
typedef struct tty {
    char input_buf[512];
    size_t input_len;

    int echo;
    int canonical;      // line mode
    pid_t fg_pgid;      // foreground process group

} tty_t;
````

Each terminal has:

* Input buffer
* Mode flags
* Foreground process group

Initially support a single system console TTY.

## Input Flow

### Raw Input

Keyboard driver delivers raw characters to TTY layer instead of directly to FD 0.

### Canonical Mode (Default)

Behavior:

* Characters buffered until Enter pressed
* Backspace edits buffer
* Ctrl+U clears current line
* Enter submits line to waiting process
* Echo characters to console

### Non-Canonical Mode (Optional)

Optional flag:

* Deliver characters immediately
* No line buffering

Can be used later for full-screen programs.

## STDIN Integration

FD 0 is attached to TTY device.

SYS_read on FD 0:

* Blocks until TTY has data ready
* Returns one full line in canonical mode
* Returns raw characters in non-canonical mode

## Echo Handling

TTY layer controls echo:

* Kernel console output used for echo
* Applications no longer echo manually

Shell should disable manual echoing.

## Job Control Model

### Process Groups

Extend process structure with:

```c
pid_t pgid;
```

Shell behavior:

* When launching job:

  * Assign new process group
  * Set as foreground group

Child processes inherit pgid.

## Foreground vs Background Jobs

### Foreground Job

Shell:

* Sets tty->fg_pgid
* Waits for job completion
* Receives terminal signals

### Background Job

Shell syntax:

```
command &
```

Behavior:

* Do not wait
* Do not assign foreground control
* Shell prompt returns immediately

Background job does not receive keyboard input.

## Signal Routing Rules

Ctrl+C:

* Send SIGINT to foreground process group

Ctrl+Z:

* Send SIGSTOP to foreground process group

SIGSTOP behavior:

* Stop all processes in group
* Change state to PROC_STOPPED

SIGCONT:

* Resume stopped job
* Mark READY
* Restore scheduling

## Shell Built-in Job Commands

Implement:

### jobs

Lists background/stopped jobs:

```
[1] Running cmd
[2] Stopped cmd
```

### fg

Syntax:

```
fg <job_id>
```

Behavior:

* Bring job to foreground
* Assign tty fg_pgid
* Wait for completion

### bg (optional)

Syntax:

```
bg <job_id>
```

Behavior:

* Resume stopped job in background

## Process State Extensions

Extend process state enum:

```c
PROC_STOPPED
```

Scheduler behavior:

* Do not schedule STOPPED processes
* Resume on SIGCONT

## Validation Tests

### TEST1: Line Editing

Shell input:

* Type characters
* Use backspace
* Ctrl+U clears line

Expected:

* Correct on-screen behavior
* Correct input delivered to shell

### TEST2: Ctrl+Z Stop

Run long program.

Press Ctrl+Z.

Expected:

* Program stops
* Shell prints job stopped message
* Prompt returns

### TEST3: Foreground Resume

Run:

```
fg 1
```

Expected:

* Job resumes
* Terminal control restored
* Ctrl+C works again

### TEST4: Background Job

Run:

```
loop &
```

Expected:

* Shell prompt returns immediately
* loop continues running
* Ctrl+C does not kill background job

### TEST5: Signal Routing

Verify:

* Ctrl+C affects only foreground job
* Shell unaffected

## Safety Requirements

* Never deliver terminal signals to kernel tasks
* Prevent orphaned foreground groups
* Validate job IDs
* Prevent scheduler starvation
* Avoid TTY buffer overflow

## Implementation Notes

### Suggested Files

New:

* include/tty.h
* src/tty.c

Modified:

* src/kbd.c
* src/syscall.c
* src/scheduler.c
* src/signal.c
* userland shell

### Blocking Behavior

SYS_read on TTY:

* Must block using scheduler
* Must wake on newline arrival

### Job Table

Shell maintains:

* Job list
* Maps job IDs to PGIDs

Kernel only tracks PGIDs and states.

## Acceptance Criteria

Prototype 24 is complete when:

* TTY handles line buffering and echo
* Ctrl+C and Ctrl+Z behave correctly
* Foreground/background jobs work
* Shell job commands function
* Interactive use feels stable
* No kernel crashes under job control stress

## Next Prototype (Planned)

Prototype 25: Memory-Mapped Files and mmap()

Goals:

* Implement mmap syscall
* Map files into user memory
* Enable shared memory regions
* Prepare for advanced user applications

```
```
