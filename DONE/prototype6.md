# Prototype 6: Cooperative Multitasking

## Purpose

Introduce multiple kernel execution contexts (tasks) and voluntary context switching without timer-based preemption. This prototype establishes the foundation for scheduling while keeping control flow deterministic and debuggable.

Tasks explicitly yield control to the scheduler.

## Scope

In scope:
- Task data structures
- Context save and restore
- Task creation API
- Voluntary yield mechanism
- Round-robin cooperative scheduler
- Idle task integration

Out of scope:
- Preemptive scheduling
- Timer-driven context switches
- User mode processes
- Separate address spaces
- Priority scheduling
- SMP support

## Dependencies

Required components:
- Working timer and sleep API (Prototype 5)
- Stable interrupt and exception handling
- Kernel heap (kmalloc/kfree)
- panic() and ASSERT macro
- CPU context save/restore primitives

## Functional Requirements

### 1. Task Representation

Each task must store at minimum:

- Stack pointer (RSP)
- Instruction pointer (RIP) or saved return address
- Callee-saved registers:
  - RBX, RBP, R12–R15
- Task state:
  - RUNNING
  - READY
  - FINISHED

Recommended structure:

```c
typedef struct task {
    uint64_t rsp;
    struct task *next;
    int state;
} task_t;
````

Additional fields may be added later.

### 2. Stack Allocation

Each task must have its own stack:

* Allocate stack memory from kernel heap
* Minimum size: 16 KiB recommended
* Stack must be 16-byte aligned

Stacks must not overlap.

### 3. Task Creation API

Provide:

```c
task_t *task_create(void (*entry)(void));
```

Behavior:

* Allocate task structure
* Allocate stack
* Initialize stack so that first context switch jumps to entry()
* Task starts execution when scheduled

Entry function returns:

* Mark task as FINISHED
* Yield control to scheduler

### 4. Context Switch Mechanism

Implement low-level context switch:

```c
void context_switch(task_t *old, task_t *new);
```

Must:

* Save callee-saved registers and stack pointer of old task
* Restore registers and stack pointer of new task
* Return execution into new task context

This is typically done in assembly.

### 5. Scheduler

Implement a simple round-robin scheduler:

* Maintain linked list of runnable tasks
* Current task pointer
* scheduler_yield() function switches to next READY task

Provide:

```c
void scheduler_init(void);
void scheduler_add(task_t *task);
void scheduler_yield(void);
```

Rules:

* Do not schedule FINISHED tasks
* Skip tasks not READY
* Always keep an idle task available

### 6. Idle Task

Implement idle task:

* Executes hlt in a loop
* Always READY
* Used when no other tasks are runnable

Idle task must never terminate.

### 7. Cooperative Yield API

Provide:

```c
void task_yield(void);
```

Behavior:

* Calls scheduler_yield()
* Allows current task to give up CPU voluntarily

### 8. Interrupt Safety

During context switch:

* Disable interrupts (cli)
* Perform switch
* Restore interrupt state after switch

Avoid switching contexts inside interrupt handlers.

## Kernel Validation Tests

### Test 1: Two Task Alternation

Create two tasks:

Task A:

* Prints "A"
* timer_sleep_ms(500)
* yield
* repeat 5 times

Task B:

* Prints "B"
* timer_sleep_ms(500)
* yield
* repeat 5 times

Expected output:

Alternating A/B messages over time.

### Test 2: Task Exit Handling

Create task that:

* Prints "done"
* Returns

Scheduler must:

* Remove task from runnable list
* Continue running remaining tasks

### Test 3: Idle Fallback

After all tasks exit:

* System should fall back to idle task
* Timer ticks continue
* No crash or lockup

## Implementation Notes

### Assembly Context Switch

Save registers:

* RBX
* RBP
* R12
* R13
* R14
* R15
* RSP

Do not save volatile registers (RAX, RCX, RDX, RSI, RDI).

### Stack Bootstrap

When creating a task stack:

* Push fake return address pointing to task exit handler
* Push initial register frame
* Set RSP to prepared stack top

### Memory Safety

* Assert stack alignment
* Assert task pointer validity
* Zero stack memory optionally for debugging

## Acceptance Criteria

Prototype 6 is complete when:

* Multiple kernel tasks run cooperatively
* Yield switches context correctly
* No stack corruption or faults occur
* Finished tasks are cleaned up
* Idle task runs when appropriate
* Timing behavior remains stable

## Next Prototype (Planned)

Prototype 7: User mode + syscall interface

Focus areas:

* Ring 3 transition
* SYSCALL/SYSRET entry
* Per-task user context
* Basic syscalls (exit, write, yield)

