````md
# Prototype 23: Signals, Ctrl+C Handling, and Basic Job Control

## Purpose

Introduce asynchronous signal delivery and basic job control semantics. This allows the terminal to interrupt running programs (Ctrl+C), terminate processes cleanly, and establish the groundwork for foreground/background execution control.

After this prototype:

- Users can interrupt programs
- Kernel can asynchronously notify processes
- Shell gains control over running jobs
- Process termination becomes event-driven instead of cooperative only

## Scope

In scope:
- Signal delivery framework
- SIGINT (Ctrl+C) support
- Process termination via signal
- Foreground job tracking
- Shell integration for job control basics

Out of scope:
- Full POSIX signal set
- Custom signal handlers
- Signal masking
- SIGSTOP/SIGCONT job control
- Background job execution (&)
- Terminal line discipline

## Dependencies

Required components:
- Preemptive scheduler (Prototype 17)
- Process lifecycle and wait() (Prototype 15)
- Userland shell (Prototype 20)
- Keyboard driver (Prototype 12)
- FD layer (Prototype 21)
- Pipes and redirection (Prototype 22)

## Signal Model

### Supported Signals (Prototype 23)

Implement minimal set:

- SIGINT (Interrupt, value 2)
- SIGKILL (Forced termination, value 9)

SIGKILL is kernel-only (not generated from keyboard).

## Process Signal State

Extend process structure with:

```c
uint32_t pending_signals;
uint32_t blocked_signals;   // optional stub, may be unused
````

Signals are represented as bitmask:

* Bit (1 << signal_number)

## Signal Delivery Mechanism

### send_signal(pid, sig)

Kernel function:

* Locate target process
* Set pending_signals bit
* If process is BLOCKED:

  * Move to READY state

No immediate context switch required.

## Signal Handling Semantics

### Default Actions

Prototype 23 supports default actions only:

SIGINT:

* Terminate process
* Set exit code = 128 + signal number

SIGKILL:

* Immediate termination
* Cannot be ignored

No user-level handler support in this prototype.

## Signal Check Points

Kernel must check pending signals:

* On return to user mode
* After syscall exit
* On scheduler context switch

If pending signal exists:

* Apply default action
* Kill process
* Wake parent if waiting

## Keyboard Integration (Ctrl+C)

### Detection

In keyboard driver:

* Detect Ctrl key state
* When 'C' key pressed while Ctrl active:

  * Generate SIGINT for foreground process

Do NOT print literal ^C automatically.

## Foreground Process Tracking

Shell must register foreground job:

When launching program:

* Shell sets global foreground_pid
* After program exits or is killed:

  * foreground_pid cleared

Ctrl+C behavior:

* Send SIGINT to foreground_pid only
* Shell itself must not receive SIGINT

## Kernel-Side Enforcement

If foreground process is kernel task:

* Ignore SIGINT

Only user processes are signal targets.

## Process Termination Path

When signal terminates process:

* Same cleanup path as exit()
* Mark ZOMBIE
* Store termination status
* Wake parent

Exit code format:

* 128 + signal number (UNIX convention)

Example:

SIGINT (2) => exit code 130

## Shell Behavior Changes

Shell must:

* Track active child PID
* Ignore Ctrl+C itself
* Print message when child interrupted:

Example:

```
[terminated by SIGINT]
```

Shell then returns to prompt.

## Validation Tests

### TEST1: Ctrl+C Interrupt

Shell:

```
loop
```

(loop is program with infinite loop)

Press Ctrl+C.

Expected:

* Program terminates immediately
* Shell returns to prompt
* No kernel crash

### TEST2: Multiple Signals

Start long-running program.

Press Ctrl+C repeatedly.

Expected:

* No duplicate crashes
* Clean termination once

### TEST3: Shell Immunity

Press Ctrl+C at shell prompt.

Expected:

* Shell not terminated
* Prompt remains

### TEST4: Blocked Process Wakeup

Program sleeps or blocks on pipe read.

Press Ctrl+C.

Expected:

* Program wakes and terminates
* No deadlock

## Safety Requirements

* Never deliver signals to kernel tasks
* Prevent invalid PID access
* Avoid race between scheduler and signal delivery
* Clear pending signals after handling
* Avoid killing idle task

## Implementation Notes

### Suggested Files

New:

* include/signal.h
* src/signal.c

Modified:

* src/scheduler.c
* src/syscall.c
* src/task.c
* src/kbd.c
* userland shell code

### Signal Check Hook

Recommended hook locations:

* Before returning to user mode (syscall exit path)
* After timer preemption decision
* After wait() wakeup

### Atomicity

Signal bitmask updates must be protected:

* Disable interrupts briefly during updates
* Keep critical section short

## Acceptance Criteria

Prototype 23 is complete when:

* Ctrl+C reliably terminates running user programs
* Shell remains responsive
* Foreground process tracking works
* Signal termination path is stable
* No deadlocks or kernel panics occur

## Next Prototype (Planned)

Prototype 24: Terminal Line Discipline and Job Control Expansion

Goals:

* Proper line buffering
* Echo control
* SIGSTOP/SIGCONT
* Background job support (&)
* fg/bg commands
* Interactive terminal behavior

```
```
