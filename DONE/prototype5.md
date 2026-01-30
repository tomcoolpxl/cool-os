````md
# Prototype 5: Time Services (Sleep and Delay)

## Purpose

Provide deterministic, interrupt-driven time services on top of the working PIT timer infrastructure from Prototype 4. This prototype introduces blocking delays using CPU halt instructions without implementing scheduling or preemption.

This stage prepares the kernel for future multitasking by validating timer-based waiting and interrupt wakeups.

## Scope

In scope:
- Timer query API
- Tick-based sleep and millisecond delay functions
- hlt-based waiting loop
- Controlled kernel timing tests
- Separation of IRQ logic from serial output

Out of scope:
- Preemptive scheduling
- Task management
- High-resolution timers
- APIC timer
- Sleep queues
- User-mode sleep APIs

## Dependencies

Required components:
- PIT timer generating periodic IRQ0
- PIC remapped and IRQ0 unmasked
- Stable IDT and IRQ dispatch
- Serial output
- panic() and ASSERT macro

## Functional Requirements

### 1. Timer State

The kernel must maintain a monotonic tick counter.

Requirements:
- Use a 64-bit unsigned integer
- Increment only inside timer IRQ handler
- Must never decrease
- Must not overflow during normal operation

Expose:

```c
uint64_t timer_get_ticks(void);
````

This function returns the current tick count.

### 2. Sleep API

Provide blocking delay functions:

```c
void timer_sleep_ticks(uint64_t ticks);
void timer_sleep_ms(uint64_t ms);
```

Behavior:

* timer_sleep_ticks blocks execution for the specified number of ticks
* timer_sleep_ms converts milliseconds to ticks using PIT frequency
* Delays must be interrupt-driven
* Busy-wait loops without hlt are forbidden

### 3. CPU Idle Behavior

Sleep implementation must:

* Keep interrupts enabled
* Use the hlt instruction while waiting
* Resume execution on timer interrupt
* Re-evaluate wake condition after each interrupt

Reference behavior:

```
target = timer_get_ticks() + delay
while (timer_get_ticks() < target)
    hlt
```

### 4. IRQ Handler Constraints

The timer interrupt handler must remain lightweight:

Allowed:

* Increment tick counter
* Send PIC EOI

Forbidden:

* Memory allocation
* Serial printing
* Blocking operations
* Complex logic

All printing must occur outside interrupt context.

### 5. Timing Accuracy Expectations

Given PIT configuration at 100 Hz:

* One tick equals approximately 10 ms
* timer_sleep_ms(1000) should block for approximately one second
* Precision is approximate and not required to be real-time accurate

## Kernel Validation Tests

During kernel initialization, the following tests must be executed.

### Test 1: Fixed Delay Test

Procedure:

* Print "sleep test start"
* Call timer_sleep_ms(1000)
* Print "wake 1"
* Repeat three times

Expected behavior:

* Output messages spaced approximately one second apart
* System remains responsive and stable

### Test 2: Chained Delay Test

Procedure:

* Call timer_sleep_ms(200)
* Call timer_sleep_ms(300)
* Call timer_sleep_ms(500)
* Print completion message

Expected behavior:

* Combined delay approximately equal to one second

### Test 3: Long Delay Stability

Procedure:

* Call timer_sleep_ms(5000)
* Print completion message

Expected behavior:

* Kernel resumes normally
* Timer continues incrementing without stalls

## Safety Requirements

* Sleep loop must not disable interrupts
* No race conditions between timer IRQ and sleep logic
* timer_get_ticks must be safe under optimization (use volatile or atomic semantics if required)

## Implementation Notes

### Suggested Files

* include/timer.h (extend existing)
* src/timer.c (extend existing)

### Suggested Variables

* static volatile uint64_t timer_ticks

### Tick Frequency

* Use PIT frequency defined during Prototype 4 initialization
* Expose tick frequency constant for ms conversion

Example:

```c
#define TIMER_HZ 100
```

### ms to ticks conversion

Recommended formula:

```
ticks = (ms * TIMER_HZ + 999) / 1000
```

This rounds up fractional ticks.

## Acceptance Criteria

Prototype 5 is complete when:

* timer_get_ticks returns increasing values
* timer_sleep_ms and timer_sleep_ticks work reliably
* CPU enters hlt state while waiting
* Serial output timing matches expected delays
* Kernel continues running after long sleep intervals
* No unexpected interrupts, faults, or ASSERT failures occur

## Deliverables

* prototype5.md (this document)
* Updated timer implementation with sleep APIs
* Kernel test code verifying delay behavior
* Updated Makefile if new sources are added

