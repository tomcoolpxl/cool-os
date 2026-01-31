# Implementation Plan: Prototype 17 - Preemptive Scheduler

## Overview

Replace cooperative multitasking with timer-driven preemptive scheduling. This allows the kernel to automatically regain control from running tasks, enforce fair time slices, and prevent CPU monopolization.

## Current State Analysis

### Existing Components
- **Timer (timer.c):** IRQ0 handler calls `pit_tick()` and sends EOI, no scheduler integration
- **Scheduler (scheduler.c):** Cooperative `scheduler_yield()` with round-robin queue, CR3 switching
- **Context Switch (context_switch.S):** Saves only callee-saved registers (RBP, RBX, R12-R15), uses `ret`
- **IRQ Stub (isr_stubs.S):** `irq_common_stub` saves ALL GPRs, returns via `iretq`
- **Task Struct (task.h):** Has state field but no time slice tracking

### Key Challenge
Cooperative and preemptive context switches differ fundamentally:
- **Cooperative:** Task calls `scheduler_yield()`, C calling convention already saved caller-saved registers. Only save callee-saved, return via `ret`.
- **Preemptive:** Task interrupted at arbitrary point. IRQ stub saves ALL registers. Must preserve full interrupt frame and return via `iretq`.

## Design Decisions

### Approach: Unified Interrupt Frame Context Switching

Unify both cooperative and preemptive paths to always use interrupt frame format:
1. IRQ preemption: frame already on stack from `irq_common_stub`
2. Cooperative yield: build synthetic interrupt frame before switching
3. All context switches restore via `iretq`

This eliminates asymmetry and simplifies the scheduler.

### Time Slice Configuration
```c
#define SCHED_TICK_SLICE 5  // 5 ticks = 50ms at 100Hz PIT
```

### Reentrancy Guard
```c
static volatile int in_scheduler = 0;  // Prevent nested scheduling
```

---

## Implementation Tasks

### Task 1: Extend Task Structure
**File:** `include/task.h`

Add fields:
```c
uint32_t ticks_remaining;  // Time slice countdown
int was_preempted;         // 1 if preempted, 0 if yielded (may not be needed with unified approach)
```

### Task 2: Create New Context Switch Assembly
**File:** `src/context_switch.S` (modify) or `src/preempt_switch.S` (new)

Create `preempt_context_switch(old, new)`:
- Takes RSP pointing to interrupt frame on old task's stack
- Saves old RSP to `old->rsp`
- Loads new RSP from `new->rsp`
- Returns (caller handles the pop/iretq)

This is similar to existing but designed to work mid-IRQ.

### Task 3: Modify Timer IRQ Handler
**File:** `src/timer.c`

```c
void irq_handler(struct interrupt_frame *frame) {
    if (frame->vector == IRQ_TIMER) {
        pit_tick();

        // Time slice accounting
        if (current_task && current_task->state == PROC_RUNNING) {
            if (current_task->ticks_remaining > 0) {
                current_task->ticks_remaining--;
            }
            if (current_task->ticks_remaining == 0) {
                // Mark for preemption - will switch after EOI
                scheduler_preempt(frame);  // Pass frame for RSP calculation
            }
        }

        pic_send_eoi(0);
    }
    // ... keyboard handler unchanged
}
```

### Task 4: Add Preemptive Scheduler Entry Point
**File:** `src/scheduler.c`

```c
// Called from timer IRQ when time slice expires
// frame points to interrupt frame on current stack
void scheduler_preempt(struct interrupt_frame *frame) {
    if (in_scheduler) return;  // Prevent reentrancy
    in_scheduler = 1;

    // Calculate RSP pointing to top of interrupt frame
    // (frame is passed by irq_handler, RSP = (uint64_t)frame)
    uint64_t saved_rsp = (uint64_t)frame;

    task_t *old = current_task;
    task_t *next = find_next_ready_task(old);

    if (old != next) {
        // Save current context
        old->rsp = saved_rsp;
        if (old->state == PROC_RUNNING) {
            old->state = PROC_READY;
        }

        // Reset time slice for next task
        next->ticks_remaining = SCHED_TICK_SLICE;
        next->state = PROC_RUNNING;
        current_task = next;

        // Switch CR3 if needed
        if (next->cr3 != 0 && next->cr3 != (read_cr3() & PTE_ADDR_MASK)) {
            write_cr3(next->cr3);
        }

        // Set TSS RSP0 for user tasks
        if (next->is_user && next->kernel_rsp) {
            tss_set_rsp0(next->kernel_rsp);
        }

        // Switch stacks - this modifies RSP
        // irq_common_stub will pop from new stack and iretq
        switch_stack_preempt(old, next);
    }

    in_scheduler = 0;
}
```

### Task 5: Assembly Stack Switch for Preemption
**File:** `src/isr_stubs.S` or new `src/preempt_switch.S`

```asm
.global switch_stack_preempt
# void switch_stack_preempt(task_t *old, task_t *new)
# Called from scheduler_preempt while in IRQ context
# RSP currently points to old task's interrupt frame
# After this, RSP points to new task's saved context
switch_stack_preempt:
    # old->rsp already saved by scheduler_preempt
    # Load new->rsp (new task's saved RSP)
    movq (%rsi), %rsp   # RSP = new->rsp (offset 0 in task_t)
    ret                 # Return to scheduler_preempt, then back to irq_common_stub
```

Wait, this needs more thought. The stack switch must happen so that when we return from `irq_handler`, the `irq_common_stub` pops registers from the NEW task's stack.

**Revised approach:** Modify `irq_common_stub` to handle preemption.

### Task 5 (Revised): Modify IRQ Common Stub
**File:** `src/isr_stubs.S`

Add preemption check after `irq_handler` returns:

```asm
irq_common_stub:
    cld
    # Save all GPRs
    pushq %rax
    pushq %rbx
    # ... (existing pushes)
    pushq %r15

    # Pass frame pointer to handler
    movq %rsp, %rdi
    call irq_handler

    # Check if preemption occurred (scheduler changed RSP)
    # Load current_task->rsp and compare/switch
    movq current_task(%rip), %rax
    movq (%rax), %rsp              # Load possibly-updated RSP

    # Restore GPRs (from new task's stack if switched)
    popq %r15
    # ... (existing pops)
    popq %rax

    addq $16, %rsp  # Skip vector and error_code
    iretq
```

Actually, this approach has a problem: we'd need to reload RSP after `irq_handler` returns, which means `irq_handler` (or `scheduler_preempt`) must update `current_task->rsp` with the correct value for the new task.

**Cleaner approach:** Have `scheduler_preempt` directly manipulate the stack.

### Task 5 (Final): Scheduler Preempt with Direct Stack Manipulation
**File:** `src/scheduler.c`

```c
void scheduler_preempt(struct interrupt_frame *frame) {
    // ... find next task ...

    if (old != next) {
        old->rsp = (uint64_t)frame;  // Save old stack pointer

        // ... CR3 switch, TSS update ...

        // The key: we're still in IRQ handler
        // When we return, irq_common_stub will pop regs and iretq
        // We need to switch stacks NOW so those pops come from new task's stack

        // This is done via inline assembly that:
        // 1. Saves current RBP (we're in a C function)
        // 2. Loads RSP from next->rsp
        // 3. Returns (pops into new task's context)

        // But wait - we can't just return, we're inside irq_handler -> scheduler_preempt
        // Return stack would be wrong
    }
}
```

This is getting complicated. Let me redesign.

### Revised Design: Tail-Call to Switch

The cleanest approach is to have the preemption switch happen at the end of `irq_common_stub` by checking a flag:

```asm
irq_common_stub:
    # ... save all GPRs ...
    movq %rsp, %rdi
    call irq_handler

    # After handler returns, check if we need to switch tasks
    # preempt_pending is set by scheduler if switch needed
    movq preempt_new_rsp(%rip), %rax
    testq %rax, %rax
    jz .no_switch

    # Switch to new task's stack
    movq %rax, %rsp
    movq $0, preempt_new_rsp(%rip)  # Clear flag

.no_switch:
    # Pop GPRs from current stack (may be switched)
    popq %r15
    # ...
    iretq
```

And in `scheduler_preempt()`:
```c
extern uint64_t preempt_new_rsp;  // Global, checked by irq_common_stub

void scheduler_preempt(struct interrupt_frame *frame) {
    // ... find next task ...
    old->rsp = (uint64_t)frame;
    // ... switches ...
    preempt_new_rsp = next->rsp;  // Signal to irq_common_stub
}
```

This is clean and works.

---

## Revised Implementation Plan

### Phase 1: Infrastructure Changes

#### 1.1 Extend task_t structure
**File:** `include/task.h`
- Add `uint32_t ticks_remaining`

#### 1.2 Add scheduler constants
**File:** `include/scheduler.h`
- Add `#define SCHED_TICK_SLICE 5`
- Declare `void scheduler_preempt(struct interrupt_frame *frame)`

#### 1.3 Add preemption signal variable
**File:** `src/scheduler.c`
- Add `volatile uint64_t preempt_new_rsp = 0` (global, exported)
- Add `static volatile int in_scheduler = 0` (reentrancy guard)

### Phase 2: Core Implementation

#### 2.1 Modify IRQ common stub
**File:** `src/isr_stubs.S`

After `call irq_handler`, add:
```asm
    # Check for preemption
    movq preempt_new_rsp(%rip), %rax
    testq %rax, %rax
    jz .no_preempt_switch
    movq %rax, %rsp
    movq $0, preempt_new_rsp(%rip)
.no_preempt_switch:
```

#### 2.2 Implement scheduler_preempt()
**File:** `src/scheduler.c`

New function that:
1. Checks reentrancy guard
2. Saves current RSP (frame pointer) to current task
3. Marks current task as READY
4. Finds next READY task (round-robin)
5. Resets time slice for new task
6. Switches CR3 if needed
7. Updates TSS RSP0 for user tasks
8. Sets `preempt_new_rsp = next->rsp`

#### 2.3 Modify timer IRQ handler
**File:** `src/timer.c`

In `irq_handler()` for IRQ_TIMER:
1. Call `pit_tick()` (keep existing)
2. Decrement `current_task->ticks_remaining`
3. If zero, call `scheduler_preempt(frame)`
4. Send EOI (after potential preemption setup)

### Phase 3: Cooperative Yield Integration

#### 3.1 Modify scheduler_yield() for consistency
**File:** `src/scheduler.c`

Two options:
- **Option A:** Keep existing callee-saved mechanism (simpler, works alongside preemption)
- **Option B:** Unify with interrupt frame approach (cleaner but more work)

**Choose Option A** for initial implementation. The key is that both paths correctly save/restore task state.

For cooperative yield:
- Save callee-saved regs via `context_switch()` as before
- Set time slice before switching to new task

#### 3.2 Initialize time slice on task creation
**Files:** `src/task.c`, `src/scheduler.c`

- In `task_create()`, `task_create_user()`, `task_create_elf()`: set `ticks_remaining = SCHED_TICK_SLICE`
- In `scheduler_init()`: set bootstrap task's time slice

### Phase 4: Safety and Edge Cases

#### 4.1 Idle task handling
- Idle task should never be preempted into itself
- Idle task gets unlimited time slice (or reset on each schedule)

#### 4.2 Blocked/Zombie handling
- Preemption should never happen to BLOCKED or ZOMBIE tasks
- Timer handler checks `state == PROC_RUNNING` before preempting

#### 4.3 Kernel task preemption
- Initially, allow preemption of kernel tasks
- Later prototype can add kernel preemption disable zones

---

## Test Plan

### Unit Tests (regtest suite: "preempt")

#### Test 1: preempt_basic
- Create task that loops forever
- Verify it gets preempted (timer still fires)
- Verify other tasks run

#### Test 2: preempt_timeslice
- Create task that tracks tick count
- Verify it runs for approximately SCHED_TICK_SLICE ticks before switch

#### Test 3: preempt_fairness
- Create 3 tasks, each increments a counter in tight loop
- After N ticks, verify all counters approximately equal (within tolerance)

#### Test 4: preempt_yield_compat
- Verify explicit `task_yield()` still works
- Mix preemption and voluntary yields

#### Test 5: preempt_user_mode
- Create user-mode task with infinite loop
- Verify kernel preempts it
- Verify user task resumes correctly

#### Test 6: preempt_blocked_skip
- Create task that blocks (e.g., wait)
- Verify scheduler skips it during preemption

#### Test 7: preempt_stress
- Create many tasks, run for extended time
- Verify no crashes, stack corruption, or triple faults

### Interactive Tests (manual validation)

#### Test: Shell Responsiveness
- Run `LOOP.ELF` (CPU hog program, new user program)
- Type shell commands while it runs
- Verify shell remains responsive

#### Test: Multi-Program Fairness
- Launch multiple user programs simultaneously
- Verify interleaved output

---

## New Files

### User program: `user/loop.c`
CPU hog for testing preemption:
```c
#include <stdio.h>
int main(void) {
    int i = 0;
    while (1) {
        if (i % 10000000 == 0) {
            printf("loop: %d\n", i / 10000000);
        }
        i++;
    }
    return 0;
}
```

### User program: `user/spinner.c`
Prints its ID periodically (for fairness test):
```c
#include <stdio.h>
#include <unistd.h>
int main(void) {
    int pid = getpid();
    for (int i = 0; i < 5; i++) {
        printf("spinner[%d]: tick %d\n", pid, i);
        // No yield - relies on preemption
        volatile int delay = 0;
        while (delay < 5000000) delay++;
    }
    return 0;
}
```

---

## Modified Files Summary

| File | Changes |
|------|---------|
| `include/task.h` | Add `ticks_remaining` field |
| `include/scheduler.h` | Add `SCHED_TICK_SLICE`, declare `scheduler_preempt()` |
| `src/scheduler.c` | Add `scheduler_preempt()`, preemption globals, time slice init |
| `src/timer.c` | Add time slice decrement and preemption trigger |
| `src/task.c` | Initialize `ticks_remaining` in task creation functions |
| `src/isr_stubs.S` | Add preemption check after `irq_handler` returns |
| `tests/regtest_suites.c` | Add preemption test suite |
| `include/regtest.h` | Add `REGTEST_PREEMPT` flag and `regtest_preempt()` |
| `user/loop.c` | New: CPU hog test program |
| `user/spinner.c` | New: Fairness test program |
| `CLAUDE.md` | Document Proto 17 completion |
| `DONE/prototype17.md` | Move from TODO after completion |

---

## Implementation Order

1. **Phase 1:** Infrastructure (task.h, scheduler.h updates)
2. **Phase 2a:** Assembly stub modification (isr_stubs.S)
3. **Phase 2b:** scheduler_preempt() implementation
4. **Phase 2c:** Timer handler integration
5. **Phase 3:** Time slice initialization in task creation
6. **Phase 4:** Test user programs (loop.c, spinner.c)
7. **Phase 5:** Regtest suite implementation
8. **Phase 6:** Integration testing and debugging
9. **Phase 7:** Documentation update

## Acceptance Criteria

- [ ] Timer interrupts preempt running tasks after time slice expires
- [ ] CPU hog tasks no longer freeze the system
- [ ] Fair round-robin scheduling works (approximately equal CPU time)
- [ ] Shell remains responsive during heavy task load
- [ ] Cooperative `task_yield()` still functions correctly
- [ ] Blocked tasks are not scheduled
- [ ] System runs stably for extended time (minutes) without fault
- [ ] All existing regtests pass
- [ ] New preemption regtests pass
