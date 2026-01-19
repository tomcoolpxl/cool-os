```md
# Prototype 17: Preemptive Scheduler and Timer-Based Multitasking

## Purpose

Replace cooperative multitasking with true preemptive scheduling driven by timer interrupts. This allows the kernel to regain control automatically, enforce time slices, and prevent misbehaving tasks from monopolizing the CPU.

This prototype completes the transition from a cooperative demo OS to a real multitasking kernel.

## Scope

In scope:
- Timer interrupt driven preemption
- Time slice accounting
- Scheduler invocation from IRQ context
- Safe context switching under interrupt
- Ready queue management
- Idle task handling

Out of scope:
- SMP support
- CPU affinity
- Priority scheduling
- Real-time scheduling
- Tickless kernel
- Power management

## Dependencies

Required components:
- PIT timer + IRQ0 handler (Prototype 4/5)
- Stable IDT/ISR infrastructure
- Per-process address spaces (Prototype 16)
- Process lifecycle + states (Prototype 15)
- Context switch mechanism
- TSS and kernel stack switching
- Working syscall path

## Scheduling Model

### Scheduling Policy

Use simple round-robin scheduling:

- Fixed time slice per process
- FIFO ready queue
- No priorities

### Time Slice

Define:

```

SCHED_TICK_SLICE = 5   // example: 5 timer ticks

```

At 100 Hz PIT:

- Time slice ≈ 50 ms

This is adequate for testing.

## Timer Integration

Modify timer IRQ handler:

Current behavior:

- Increment tick counter
- Send EOI

New behavior:

- Increment current process tick counter
- If time slice expired:
  - Mark current process READY
  - Invoke scheduler

Do NOT perform heavy logic inside IRQ handler.

Scheduler call must:

- Save context of current task
- Pick next READY task
- Switch CR3
- Switch kernel stack (TSS already handled)
- Restore context
- Return via iretq

## Context Switch Path

Preemptive switch occurs from:

- IRQ0 interrupt frame

Requirements:

- Save full register context
- Preserve user RIP/RSP/FLAGS
- Correctly resume in new process context

Reuse existing context switch mechanism used for yield if possible.

## Critical Section Protection

Scheduler data structures must be protected:

- Disable interrupts (cli) during runqueue modifications
- Re-enable after switch decision

Avoid holding interrupts disabled for long periods.

## Idle Task

Provide a kernel idle task:

Behavior:

- Executes hlt in loop
- Runs when no READY processes exist

Idle task must never be preempted into itself.

## Blocking Behavior Integration

When process enters:

- PROC_BLOCKED (wait, sleep later)

Scheduler must:

- Skip blocked processes
- Only schedule READY ones

When blocked process becomes READY:

- Insert back into run queue

## Syscall Interaction

SYS_yield becomes optional:

- Can still voluntarily yield
- But preemption enforces fairness

Ensure syscalls are reentrant-safe under preemption.

## Validation Tests

### TEST1: CPU Hog Program

User program:

- Infinite loop printing periodically without calling yield

Expected:
- Kernel still preempts
- Shell remains responsive
- Other tasks continue running

### TEST2: Multiple Programs Fairness

Launch 3 user programs printing their ID.

Expected:
- Interleaved output
- No starvation

### TEST3: Shell Responsiveness

While user program runs:

- Type commands
- Use shell

Expected:
- Input responsive
- No freezes

### TEST4: Stability Stress

Launch and terminate programs repeatedly.

Expected:
- No crashes
- No corrupted stacks
- No triple faults

## Safety Requirements

- No context switch while holding spinlocks or interrupts disabled
- Always send PIC EOI before switching
- Prevent re-entrant scheduler invocation
- Validate process state transitions

## Implementation Notes

### Reentrancy Guard

Use per-CPU or global flag:

```

in_scheduler = true/false

```

To prevent nested scheduling.

### Accounting Fields

Extend process struct:

- uint32_t ticks_remaining;

Reset on scheduling.

### Timer Handler Flow

Pseudo logic:

```

on_timer_irq():
ticks++
current->ticks_remaining--
if ticks_remaining == 0:
schedule()
send_eoi()

```

Send EOI before switching or immediately after IRQ entry depending on your ISR design.

### Context Switch Implementation

Prefer:

- Common switch routine usable by both yield and preemption
- Avoid duplicating logic

## Deliverables

Modified:

- src/timer.c
- src/scheduler.c
- src/task.c
- src/isr.c or irq handler glue
- include/task.h
- include/scheduler.h

Optional new:

- src/idle.c
- include/idle.h

## Acceptance Criteria

Prototype 17 is complete when:

- Kernel preempts tasks using timer interrupts
- CPU hog tasks no longer freeze the system
- Fair round-robin scheduling works
- Shell remains responsive during heavy task load
- System runs stably for extended time (minutes) without fault

## Next Prototype (Planned)

Prototype 18: Fork and Copy-on-Write Preparation

Goals:
- Implement fork()
- Duplicate address spaces
- Introduce reference-counted physical pages
- Prepare for UNIX-like process model
```
