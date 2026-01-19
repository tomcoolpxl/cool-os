```md
# Prototype 18: fork(), Address Space Duplication, and Copy-on-Write Preparation

## Purpose

Introduce UNIX-style process creation by implementing fork() and preparing the memory subsystem for copy-on-write (COW). This prototype enables parent processes to spawn children efficiently while sharing memory pages safely.

This is a major semantic milestone: true process cloning and parallel execution of identical address spaces.

## Scope

In scope:
- fork() syscall
- Process duplication
- Address space cloning
- Shared page tracking
- Reference counting for physical pages
- Page fault handling groundwork for COW
- Read-only page remapping for shared memory

Out of scope:
- execve() replacement model
- Demand paging
- Swap
- Memory overcommit
- vfork()
- POSIX signal semantics

## Dependencies

Required components:
- Per-process address spaces (Prototype 16)
- Preemptive scheduler (Prototype 17)
- Process lifecycle + wait() (Prototype 15)
- Paging subsystem with map/unmap helpers
- Page fault handler with error code decoding
- Kernel heap and PMM

## fork() Semantics

### Behavior

fork() creates a new child process such that:

- Child receives a copy of parent's address space
- Child receives a copy of parent's registers
- Parent and child continue execution from same point
- Return value:
  - Parent receives child's PID
  - Child receives 0

### Inheritance Rules

Child inherits:

- Code and data mappings
- Heap contents
- Stack contents
- Open file descriptors (future prototype)

Child does NOT inherit:

- Kernel stack
- Scheduler runtime counters
- Pending wait states

## Address Space Cloning Strategy

### Phase 1 (Prototype 18)

Use eager page table cloning with shared physical pages:

For each user-mapped page:

- Map same physical frame into child address space
- Clear writable bit (W=0)
- Set both parent and child mappings to read-only
- Mark page as COW candidate internally

Kernel mappings are copied normally.

### No Immediate Copy

Physical memory is not duplicated during fork.

Duplication only happens on write fault.

## Physical Page Reference Counting

Extend PMM or page frame metadata with:

- refcount field per physical frame

Behavior:

- On mapping into another address space: increment refcount
- On unmapping: decrement refcount
- On free: only free frame when refcount reaches zero

Frame metadata can be stored:

- In a parallel array indexed by frame number
- Or embedded in PMM bitmap extension

## Page Fault Handling (COW Trigger)

Extend page fault handler:

When fault occurs:

If:
- Fault is write access
- Page is marked read-only
- Page belongs to COW set

Then:

1) Allocate new physical frame
2) Copy old frame contents into new frame
3) Remap faulting virtual page to new frame
4) Set writable flag
5) Decrement old frame refcount
6) Resume execution

If not COW-related:

- Treat as regular fault (kill process)

## Page Metadata Tracking

Maintain per-page flags:

- COW flag
- Writable
- User-accessible

May use:

- Software shadow page table metadata
- Bit flags stored in page table unused bits (if supported)

Do NOT rely solely on hardware PTE flags.

## Kernel Stack and Context Setup

Child process must receive:

- New kernel stack
- Copy of parent's saved context frame
- Adjusted return value register (RAX = 0)

Parent receives fork return value after scheduler resumes.

## Scheduler Integration

After fork:

- Child inserted into READY queue
- Parent continues normally
- Preemption applies equally

## wait() Compatibility

Parent must be able to:

- wait() for child
- Collect exit status
- Properly clean child resources

No special handling required beyond Prototype 15.

## Validation Tests

### TEST1: Basic fork()

User program:

```

pid = fork();
if (pid == 0)
printf("child\n");
else
printf("parent\n");

```

Expected:
- Both messages printed
- Correct return values

### TEST2: Memory Independence

User program:

```

int x = 5;
if (fork() == 0) x = 10;
printf("%d\n", x);

```

Expected:
- Parent prints 5
- Child prints 10

Verifies COW correctness.

### TEST3: Stress Fork

Loop:

- Fork 10 children
- Each prints ID and exits
- Parent waits for all

Expected:
- No memory leaks
- No crashes
- Correct ordering not required

### TEST4: Write Fault Handling

Force write to shared page immediately after fork.

Expected:
- Page fault handled
- New frame allocated
- Process continues normally

## Safety Requirements

- Never allow both processes to write shared page without copy
- Ensure atomic refcount updates
- Prevent double-free of frames
- Validate all fault paths
- Avoid infinite fault loops

## Implementation Notes

### Page Table Traversal

During fork:

- Walk parent's page tables
- Duplicate PML4/PDP/PD/PT structures
- Share leaf PTE physical frames

Avoid copying kernel region.

### Flags Handling

When marking COW pages:

- Clear writable bit
- Leave present and user bits intact
- Set NX appropriately

### Performance

Prototype 18 favors correctness over performance.

Optimizations such as lazy PTE allocation or batching can be added later.

## Deliverables

Modified:

- src/syscall.c (fork syscall)
- src/paging.c (clone helpers)
- src/task.c
- src/scheduler.c
- src/isr.c (page fault handler)

New:

- include/cow.h (optional abstraction)
- src/cow.c (optional)

## Acceptance Criteria

Prototype 18 is complete when:

- fork() works correctly
- Parent and child have isolated writable memory
- COW triggers correctly on write
- No memory corruption occurs
- System remains stable under repeated fork usage

## Next Prototype (Planned)

Prototype 19: execve() and Program Replacement

Goals:
- Replace process image with new ELF
- Implement exec() syscall
- Combine fork + exec for full UNIX process model
- Prepare for moving shell fully into userland
```
