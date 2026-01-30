````md
# Prototype 15: Process Lifecycle, Parent/Child Model, and wait()

## Purpose

Introduce a real process lifecycle model with parent/child relationships and blocking waits. This prototype formalizes task management beyond cooperative scheduling and prepares the kernel for preemptive multitasking and shell job control.

It replaces the current “fire-and-forget” task model with a structured process system.

## Scope

In scope:
- Process states
- Parent-child relationships
- Exit status tracking
- wait() / waitpid()-style syscall
- Zombie process handling
- Cleanup of task resources
- Kernel shell integration

Out of scope:
- Signals
- Process groups
- Session control
- Fork (next prototype)
- Preemptive scheduling (Prototype 17)

## Dependencies

Required components:
- Cooperative scheduler
- User mode tasks and syscalls
- Kernel shell (Prototype 13)
- Basic libc (Prototype 14)
- Timer + IRQ infrastructure
- Kernel heap

## Process Model

### Process States

Add enum:

```c
typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
} proc_state_t;
````

### Process Structure Extensions

Extend task/process structure with:

* pid (unique integer)
* ppid (parent pid)
* state
* exit_code
* wait_queue or single waiting parent pointer

Kernel must maintain:

* Global process table or list
* Next PID counter

PID 1 reserved for init/shell process.

## Exit Semantics

### User exit syscall

SYS_exit(code):

Behavior:

* Store exit_code
* Change state to PROC_ZOMBIE
* Wake parent if waiting
* Release user resources:

  * User stack
  * User memory mappings (if isolated later)
* Keep minimal task structure until parent collects status

### Kernel thread exit (optional)

Kernel tasks may exit without creating zombies (simplify by exempting kernel tasks).

## wait() System Call

Add syscall:

```
int wait(int *status);
```

Or:

```
int waitpid(int pid, int *status);
```

Phase 1 requirements:

* wait() waits for any child
* Blocks calling process until a child exits
* Copies exit status into user memory

Return value:

* PID of exited child
* -1 if no children exist

### Blocking Behavior

When parent calls wait():

If a zombie child exists:

* Return immediately
* Reap child

If no zombie but children exist:

* Set parent state to PROC_BLOCKED
* Scheduler switches to another task

If no children:

* Return error immediately

### Wakeup Path

When child exits:

* If parent waiting:

  * Wake parent
  * Move parent to READY
* Keep zombie until parent collects it

## Scheduler Integration

Scheduler must:

* Skip PROC_ZOMBIE tasks
* Skip PROC_BLOCKED tasks
* Only run READY tasks

State transitions:

READY -> RUNNING
RUNNING -> BLOCKED (wait)
RUNNING -> ZOMBIE (exit)
BLOCKED -> READY (child exit)

## Resource Cleanup

When zombie is reaped:

* Free task struct
* Free kernel stack
* Free PID entry
* Remove from process list

User memory already freed at exit.

## Kernel Shell Integration

Shell behavior changes:

Current:

```
run prog.elf
```

New behavior:

* Shell becomes parent
* After launching program:

  * Option A: wait automatically (foreground job)
  * Option B: background job support later

Prototype 15 requires:

* Foreground execution:

  * shell calls wait() after launching program
  * shell blocks until program exits
  * prints exit status

## Validation Tests

### TEST1: Single Program Wait

Shell:

```
run TEST1.ELF
```

TEST1.ELF:

* Prints message
* Exits with code 42

Expected:

Shell prints:

```
Process exited with code 42
```

### TEST2: Multiple Children

Shell launches two programs sequentially:

```
run A.ELF
run B.ELF
```

Each exits with different code.

Expected:

* Shell waits for each
* No zombies left

### TEST3: Zombie Handling

Create test where:

* Parent launches child
* Does not call wait immediately
* Child exits

Verify:

* Child is zombie
* After wait(), zombie disappears
* No memory leak

### TEST4: No Children

Shell calls wait with no children:

Expected:

* Immediate return with error
* No kernel crash

## Safety Requirements

* Prevent PID reuse while zombie exists
* Prevent wait() on non-child processes
* Validate user pointer for status storage
* No deadlocks in scheduler

## Implementation Notes

### Suggested Files

Modify:

* include/task.h
* src/task.c
* src/scheduler.c
* src/syscall.c
* src/kernel.c

Add:

* include/process.h (optional abstraction layer)

### PID Allocation

Simple monotonically increasing counter is sufficient.

Wraparound handling can be deferred.

### Data Structures

Simple linked list or fixed array process table acceptable.

Maximum process count:

* 64 minimum recommended.

## Acceptance Criteria

Prototype 15 is complete when:

* Parent-child relationships are tracked
* wait() blocks and wakes correctly
* Zombies are cleaned properly
* Shell can run programs and wait for exit
* System remains stable under repeated program launches

## Next Prototype (Planned)

Prototype 16: Per-Process Address Spaces

Goals:

* Separate page tables per process
* Kernel mapped globally
* User memory isolation
* Preparation for fork()

This is the major memory management milestone.

```
```
