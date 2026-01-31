# Prototype 16: Per-Process Virtual Address Spaces and Memory Isolation

## Purpose

Introduce true virtual memory isolation by giving each user process its own page table while keeping the kernel mapped globally. This removes the single shared address space model and enforces real memory protection between processes.

This prototype is a prerequisite for fork(), demand paging, security boundaries, and robust multitasking.

## Scope

In scope:
- Per-process PML4 creation
- Kernel higher-half global mapping
- User address space isolation
- Page table cloning for process creation
- Address space switch on context switch
- Correct CR3 handling
- TLB flushing where required

Out of scope:
- Copy-on-write
- fork() syscall
- Demand paging
- Swapping
- Memory-mapped files
- ASLR

## Dependencies

Required components:
- Paging subsystem with map/unmap helpers
- ELF loader (Prototype 8/9)
- Process lifecycle and wait() (Prototype 15)
- Scheduler with task switching
- TSS and syscall infrastructure
- HHDM mapping support

## Address Space Layout

### Kernel Space

Kernel is mapped identically in all address spaces:

- Higher-half kernel mapping (example: 0xFFFFFFFF80000000+)
- Kernel stacks
- Kernel heap
- HHDM direct mapping
- Framebuffer mapping (optional)

Kernel mappings must be:

- Present
- Supervisor-only (U/S = 0)
- Global bit set if supported (G bit)

### User Space

Each process has its own:

- User code
- User data
- User heap
- User stack

User mappings must be:

- Present
- User-accessible (U/S = 1)
- NX enabled on non-executable pages

No two processes may share user memory.

## Data Structure Changes

Extend process structure:

Add:

- uint64_t cr3;           // physical address of PML4
- void *pml4_virt;        // kernel virtual mapping (HHDM)
- address_space pointer (optional abstraction)

## Address Space Creation

Provide function:

```

address_space_t *address_space_create(void);

```

Behavior:

- Allocate new PML4 page
- Zero initialize
- Copy kernel higher-half entries from kernel master PML4
- Leave lower half empty

This creates a clean user address space with kernel mapped.

## Process Creation Changes

Modify task_create_elf():

Old:
- Map user pages into global address space

New:
- Create new address space
- Map ELF segments into new address space only
- Map user stack into new address space
- Store CR3 in task structure

Kernel must no longer map user memory globally.

## Context Switch Changes

Scheduler must:

Before switching to a process:

- Load CR3 with process->cr3
- Update TSS RSP0 (already implemented)
- Flush TLB implicitly via CR3 write

When switching back to kernel idle task:

- Load kernel CR3

Note:

- Kernel page tables must always be active when no user task runs.

## Syscall Path Considerations

On SYSCALL entry:

- CPU switches to kernel CS/SS
- Kernel must already be mapped in current CR3
- HHDM and kernel stack must be valid in all address spaces

Ensure:

- Kernel code and data are mapped identically in all PML4s.

## Page Table Helpers

Extend paging API:

- map_page_pml4(pml4, virt, phys, flags)
- unmap_page_pml4(pml4, virt)
- clone_kernel_mappings(dst_pml4)

Avoid using global mapping helpers for user pages.

## Memory Cleanup on Exit

When process exits:

- Unmap and free all user pages belonging to that process
- Free page tables (walk and free)
- Preserve kernel pages

Zombie state must not retain address space mappings.

## Validation Tests

### TEST1: Address Space Isolation

Create two user programs:

- Program A writes to memory location X
- Program B reads memory location X

Expected:
- Program B does not see Program A data
- No kernel crash

### TEST2: Kernel Protection

User program attempts:

- Access kernel address (eg 0xffffffff80000000)

Expected:
- Page fault in user mode
- Process killed
- Kernel survives

### TEST3: Context Switch Stability

Run two yielding programs:

- Switch between address spaces repeatedly

Expected:
- No triple fault
- No corruption
- Stable output

### TEST4: Stress Program Creation

Repeatedly launch and exit programs.

Expected:
- No memory leaks
- Stable CR3 switching
- No stale mappings

## Safety Requirements

- Never map user pages with U/S=0
- Never map kernel pages with U/S=1
- Validate all map/unmap operations
- Always zero new page tables
- Avoid leaking physical frames

## Implementation Notes

### Kernel Master PML4

At boot:

- Save kernel PML4 physical address
- Use as template for cloning kernel entries

### Which Entries to Copy

Copy:

- Higher half kernel region entries
- HHDM region
- MMIO regions needed by kernel

Do NOT copy:

- Lower half user entries

### Performance Considerations

Prototype 16 uses full CR3 reload on every context switch.

This is acceptable.

TLB shootdown optimization is not required yet.

## Deliverables

Modified:

- include/task.h
- src/task.c
- src/paging.c
- src/scheduler.c
- src/syscall.c

New (optional):

- include/address_space.h
- src/address_space.c

## Acceptance Criteria

Prototype 16 is complete when:

- Each user process runs in its own address space
- Kernel mappings are shared and protected
- User processes cannot access each other's memory
- Syscalls and interrupts work correctly across CR3 switches
- System remains stable under repeated task creation and context switches

## Next Prototype (Planned)

Prototype 17: Preemptive Scheduler

Goals:
- Timer-driven task preemption
- Time slicing
- Remove reliance on cooperative yield
- True multitasking behavior
