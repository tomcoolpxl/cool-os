# Prototype 7: User Mode and System Call Interface

## Purpose

Introduce true user-mode execution (ring 3) and a controlled kernel entry mechanism using system calls. This prototype separates kernel and application code, enabling unprivileged programs to run safely while accessing kernel services through a defined ABI.

This is the foundation for loading real user programs, building a libc, and eventually running applications such as DOOM as user processes.

## Scope

In scope:
- Ring 3 execution (user mode)
- Per-task user context
- System call entry/exit path
- Minimal syscall dispatcher
- Safe return to user mode
- One or more simple syscalls (exit, write, yield)

Out of scope:
- Full POSIX compliance
- Signals
- Threads
- Preemptive scheduling
- Virtual memory isolation (can be single address space initially)
- File system syscalls (added later)
- mmap or demand paging

## Dependencies

Required components:
- Cooperative multitasking (Prototype 6)
- IDT and exception handling
- Working timer and sleep services
- Kernel heap
- Panic/assert infrastructure
- GDT with kernel segments
- Ability to modify or extend GDT to add user segments

## Architecture Overview

Prototype 7 introduces two execution domains:

Kernel mode (ring 0):
- Scheduler
- Interrupt handlers
- Syscall dispatcher
- Drivers

User mode (ring 3):
- Application code
- libc stubs (later)
- No direct hardware access

User code enters kernel only via SYSCALL instruction.

## Functional Requirements

### 1. GDT User Segments

Add user-mode code and data segments:

- User code segment: DPL = 3
- User data segment: DPL = 3

Kernel segments remain DPL = 0.

Selectors must be configured so that:

- CS for user mode uses ring 3 descriptor
- SS, DS, ES use user data descriptor

### 2. Per-Task User Context

Extend task structure to include:

- User RIP
- User RSP
- User RFLAGS
- Saved kernel stack pointer (for syscall entry)

Example additions:

```c
uint64_t user_rsp;
uint64_t user_rip;
uint64_t kernel_rsp;
int is_user;
````

Each task may be either:

* Kernel task
* User task

Scheduler must handle both.

### 3. SYSCALL/SYSRET Setup

Configure MSRs:

* IA32_LSTAR (syscall entry RIP)
* IA32_STAR (segment selectors)
* IA32_FMASK (flags mask)

Requirements:

* SYSCALL transitions from ring 3 to ring 0
* CPU switches to kernel code segment
* Stack switches to kernel stack for current task

SYSCALL handler must:

* Save user context
* Switch to kernel stack
* Dispatch syscall
* Return via SYSRET

### 4. Syscall ABI

Define a simple syscall convention:

Registers:

* RAX = syscall number
* RDI, RSI, RDX, R10, R8, R9 = arguments
* Return value in RAX

This matches Linux x86-64 convention and simplifies future libc compatibility.

### 5. Required Syscalls (Minimum Set)

Implement at least:

#### SYS_exit (0)

Arguments:

* RDI = exit code

Behavior:

* Mark current task FINISHED
* Yield to scheduler
* Never return to caller

#### SYS_write (1)

Arguments:

* RDI = file descriptor (only 1 = stdout supported)
* RSI = buffer pointer (user address)
* RDX = length

Behavior:

* Copy data from user memory
* Write to serial output
* Return number of bytes written

#### SYS_yield (2)

Arguments:

* none

Behavior:

* Yield execution to scheduler
* Allow cooperative multitasking from user mode

### 6. User Memory Access Safety

Initial model (simplified):

* Single address space shared between kernel and user
* User pointers assumed valid but must be range-checked
* Kernel must not trust user pointers blindly

Later prototypes may introduce separate address spaces.

### 7. User Task Creation

Provide kernel API:

```c
task_t *task_create_user(void *entry, void *user_stack_top);
```

Behavior:

* Initialize task to start in ring 3
* Setup initial user RIP and RSP
* Setup kernel stack for syscall entry
* Schedule task normally

### 8. Transition to User Mode

To start a user task:

* Load user segments
* Setup stack frame
* Use iretq or SYSRET path to enter ring 3

### 9. Exception Handling from User Mode

Requirements:

* Page faults or invalid instructions in user mode must:

  * Print diagnostic
  * Kill offending task
  * Continue scheduler

Kernel faults must still panic.

## Validation Tests

### Test 1: Hello User Program

Create a minimal user function:

* Calls SYS_write to print "hello from user"
* Calls SYS_exit

Expected behavior:

* Message appears via serial
* Kernel remains stable
* Task is cleaned up

### Test 2: Yield Test

Create two user tasks:

Each prints its ID, yields, repeats several times.

Expected:

* Alternating output
* No kernel crash
* Correct return to user mode

### Test 3: Fault Isolation

Create user task that:

* Executes invalid instruction (ud2)

Expected:

* Kernel prints user fault
* Terminates task
* System continues running

## Safety Requirements

* Kernel stack must not be reused by multiple tasks simultaneously
* Interrupts must be disabled during sensitive context transitions
* Syscall handler must validate arguments where possible
* No user code may directly access I/O ports or privileged instructions

## Implementation Notes

### Suggested Files

Add:

* include/syscall.h
* src/syscall.c
* src/syscall_entry.S

Modify:

* GDT setup code
* Task structure and scheduler
* Kernel init sequence

### Stack Layout

Each task must have:

* One kernel stack (for interrupts/syscalls)
* One user stack (for application code)

### Debugging Strategy

* Log syscall entry/exit during early testing
* Use ASSERT on invalid syscall numbers
* Print user RIP/RSP on crash

## Acceptance Criteria

Prototype 7 is complete when:

* User code executes in ring 3
* SYSCALL/SYSRET transitions work reliably
* SYS_exit, SYS_write, SYS_yield operate correctly
* User crashes do not crash kernel
* Cooperative multitasking works with mixed kernel and user tasks
* Kernel remains stable under repeated syscall use


