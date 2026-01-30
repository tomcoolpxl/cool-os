````md
# Prototype 19: execve() and Program Image Replacement

## Purpose

Introduce execve()-style process image replacement. This allows an existing process to discard its current memory image and load a new ELF program while preserving its PID and kernel-side identity.

Together with fork() (Prototype 18), this completes the classic UNIX process model:

- fork() creates a process
- execve() replaces its program image

This enables a true userland shell and structured program execution.

## Scope

In scope:
- execve() syscall
- Replacing user address space
- Loading ELF into an existing process
- Preserving kernel process metadata
- Argument passing (minimal)
- Stack reinitialization
- Error handling

Out of scope:
- Environment variables
- Shebang scripts
- PATH resolution
- Dynamic linking
- ELF interpreter support
- ASLR

## Dependencies

Required components:
- ELF loader (Prototype 8/9)
- Per-process address spaces (Prototype 16)
- fork() and COW groundwork (Prototype 18)
- VFS file loading
- Scheduler and context switching
- Userland libc entry (_start, main)
- wait() process management (Prototype 15)

## execve() Semantics

### Function Prototype

Userland API:

```c
int execve(const char *path, char *const argv[]);
````

Kernel syscall:

* SYS_execve

### Behavior

On success:

* Current process memory image is destroyed
* New ELF program is loaded into the same process
* User stack is recreated
* Instruction pointer jumps to new program entry
* PID remains unchanged
* Execution never returns to old code

On failure:

* Return -1
* Original process image remains intact

## Kernel Responsibilities

### 1. Validate Input

Before destroying old image:

* Validate user pointer to path string
* Copy path into kernel buffer
* Validate argv pointer (optional minimal support)

If validation fails:

* Return error
* Do not modify process state

## Address Space Replacement

### Old Address Space Cleanup

Before loading new program:

* Unmap all user pages belonging to process
* Release physical frames (respecting COW refcounts)
* Free page tables except kernel shared mappings

Kernel mappings remain intact.

### New Address Space Setup

Steps:

1. Create fresh address space
2. Clone kernel mappings
3. Load ELF segments into new address space
4. Map new user stack
5. Set new CR3 for process
6. Update process structure

## Stack Initialization

Minimal initial stack layout:

Prototype 19 supports:

* argc = 0
* argv = NULL

Optional enhancement:

* Push argv strings
* Push argv pointers
* Set registers according to SysV ABI

Stack alignment:

* RSP must be 16-byte aligned at entry

## Register Context Setup

Replace current process context:

Set:

* RIP = ELF entry point
* RSP = new user stack top
* RFLAGS = preserved or sanitized
* General purpose registers cleared (except ABI required)

Kernel stack remains unchanged.

## Scheduler Interaction

execve() does not create a new task:

* Current process continues execution with new image
* No scheduling changes required

If execve() is called by child after fork:

* Parent remains unaffected

## File Loading

execve() must:

* Open file using VFS
* Read ELF into kernel buffer or stream into loader
* Close file handle after mapping

Failure cases:

* File not found
* Invalid ELF
* Mapping failure
* Out of memory

All must restore original process state safely.

## Copy-On-Write Cleanup

Before destroying old mappings:

* Decrement refcounts on shared frames
* Free frames when refcount reaches zero
* Remove COW metadata entries

Avoid memory leaks.

## Validation Tests

### TEST1: Simple exec

Shell (kernel or userland):

```
exec TEST.ELF
```

Expected:

* Current shell replaced by TEST program
* TEST prints message and exits
* Parent shell (if forked) resumes correctly

### TEST2: fork + exec

User program:

```
pid = fork();
if (pid == 0)
    execve("HELLO.ELF");
else
    wait();
```

Expected:

* Child becomes HELLO program
* Parent waits correctly
* No crash

### TEST3: Failure Path

Call:

```
execve("NOFILE.ELF");
```

Expected:

* Return error
* Original program continues running

### TEST4: Memory Replacement

Old program allocates memory and writes data.

After exec:

* New program memory is clean
* No old data remains mapped

## Safety Requirements

* Never destroy old address space before ELF validation succeeds
* Prevent partial replacement state
* Validate all user pointers
* Avoid race with scheduler during replacement
* Disable preemption during critical execve stages

## Implementation Notes

### Atomic Replacement Strategy

Recommended sequence:

1. Validate ELF and load metadata
2. Create new address space
3. Load ELF into new space
4. Prepare new stack
5. Switch CR3
6. Destroy old address space
7. Install new context
8. Return to userspace at new entry

This avoids leaving process without valid memory.

### Kernel Stack Safety

Kernel stack must not depend on old address space.

Ensure kernel stacks live in kernel-only mappings.

### Reentrancy

Prevent execve from being interrupted mid-replacement:

* Disable preemption
* Disable interrupts briefly during CR3 switch

## Deliverables

Modified:

* src/syscall.c (execve syscall)
* src/task.c
* src/paging.c
* src/elf.c
* src/scheduler.c

New (optional):

* include/exec.h
* src/exec.c

Userland:

* libc execve wrapper

## Acceptance Criteria

Prototype 19 is complete when:

* execve() replaces process image correctly
* fork + exec workflow works
* PID remains unchanged
* Memory isolation preserved
* No leaks or stale mappings occur
* System remains stable under repeated exec calls

## Next Prototype (Planned)

Prototype 20: Userland Shell and Full Process Model

Goals:

* Move shell fully into userland
* Use fork + exec instead of kernel-run commands
* Implement PATH-like lookup
* Establish real stdin/stdout behavior
* Transition kernel shell to debug-only mode

```
```
