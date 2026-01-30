```md
# Prototype 3: Kernel Heap (kmalloc/kfree) Specification

## Purpose

Introduce a deterministic kernel heap allocator for the x86-64 teaching kernel. The heap must provide dynamic allocation primitives usable by future subsystems (timers, VFS, drivers) without requiring paging changes or interrupts.

This prototype builds on Prototype 2 (bitmap PMM) and uses HHDM mapping for heap memory access.

## Scope

In scope:
- Kernel heap initialization
- kmalloc / kfree API
- Simple arena-based heap growth using PMM frames
- Basic integrity checks (ASSERT, magic values)
- Deterministic heap tests at boot

Out of scope:
- User mode allocation
- Virtual memory remapping or dedicated heap virtual region
- SMP safety / locking
- Slab allocator
- Buddy allocator at heap level
- krealloc/kcalloc (optional, not required)
- Returning pages to PMM (optional future work)

## Dependencies

Required existing components:
- Serial output (COM1) for diagnostics
- panic() and ASSERT macro
- HHDM helpers:
  - phys_to_hhdm(uint64_t phys)
  - hhdm_to_phys(void *virt)
  - exported hhdm_offset
- Prototype 2 PMM API:
  - pmm_init()
  - uint64_t pmm_alloc_frame(void)
  - void pmm_free_frame(uint64_t phys) (may remain unused in Proto 3)
  - uint64_t pmm_get_free_frames(void) (optional)

## Functional Requirements

### 1. Heap API

Provide a public header (include/heap.h) declaring:

- void heap_init(void);
- void *kmalloc(uint64_t size);
- void kfree(void *ptr);

Behavior:
- kmalloc(0) returns NULL (or a unique minimal allocation if desired; choose one and document it).
- kmalloc returns a pointer aligned to 16 bytes.
- kfree(NULL) is a no-op.
- Double-free must ASSERT and halt (debug-safety requirement).

### 2. Heap Backing Strategy

The heap must be backed by physical frames from PMM:

- Allocate heap memory by calling pmm_alloc_frame().
- Access allocated frames via HHDM mapping (phys_to_hhdm).
- No page table modifications are permitted in Prototype 3.
- Heap must not require contiguous physical memory.

### 3. Arena Model

Heap storage is organized into one or more arenas.

- Each arena consists of one page initially (4 KiB).
- New arenas are added on demand when no suitable free block exists.
- Arenas form a singly linked list.

### 4. Block Model

Within each arena, heap memory is divided into blocks.

- Each block has a header immediately preceding its payload.
- Blocks form a doubly linked list within the arena.
- Allocator uses first-fit strategy across blocks.

Required behaviors:
- Split a free block when allocating, if the remainder can hold:
  - a new block header and
  - at least one aligned minimum payload chunk
- Coalesce adjacent free blocks on kfree.

### 5. Alignment

- Payload alignment: 16 bytes.
- Block headers must be sized/aligned so that returned payload pointers are 16-byte aligned.

### 6. Integrity Checks (Debug)

Heap must implement basic correctness checks:

- Block header contains a magic constant.
- ASSERT verifies:
  - magic is valid in kmalloc/kfree
  - block is not freed twice
  - size and pointer arithmetic remain within the arena
- Optional:
  - memset allocated payload to 0xAA
  - memset freed payload to 0xDD

### 7. Boot-Time Tests (Mandatory)

During kernel startup after heap_init(), run deterministic tests and print results to serial.

Minimum test set:

1) Basic alloc/free:
- a = kmalloc(32)
- b = kmalloc(64)
- ASSERT(a != NULL && b != NULL)
- kfree(a)
- kfree(b)

2) Coalescing test:
- x = kmalloc(128)
- y = kmalloc(128)
- z = kmalloc(128)
- kfree(y)
- kfree(x)
- kfree(z)
- big = kmalloc(400)
- ASSERT(big != NULL)
- kfree(big)

3) Stress test (small scale):
- allocate N small blocks (e.g. 100 x 32 bytes)
- free every other block
- allocate N/2 blocks again
- ASSERT all allocations succeed

Tests must not rely on interrupts or timers.

## Non-Functional Requirements

### Determinism
- Heap behavior must be deterministic across runs under QEMU with identical inputs.

### Safety
- Heap must not allocate from or overwrite:
  - kernel image memory
  - bootloader structures
  - PMM bitmap itself
(This is ensured by PMM correctness and reserving these regions in Prototype 2.)

### Simplicity
- Prefer clear, teachable code over performance optimizations.

### Build
- Must compile with existing kernel flags, including:
  - -ffreestanding
  - -fno-builtin
  - -fno-stack-protector
  - -mno-red-zone

## Implementation Notes

### Suggested File Additions
- include/heap.h
- src/heap.c

### Suggested Supporting Macros
- HEAP_ALIGN = 16
- ALIGN_UP(x, a)

### Suggested Structures

Arena header:
- next pointer
- total_size in bytes
- pointer to first block

Block header:
- magic
- size (payload bytes)
- free flag
- next/prev pointers

### Expansion Strategy
- heap_expand(): allocate one new page arena when needed.
- For now, do not return unused arenas/pages back to PMM.

## Acceptance Criteria

Prototype 3 is complete when:

- Kernel boots and prints heap initialization messages.
- kmalloc/kfree work for varied sizes and patterns.
- Coalescing is verified by tests (big allocation succeeds after frees).
- No page faults occur during heap operations.
- Double-free triggers ASSERT/panic.
- Tests pass under QEMU with optimization enabled (at least -O2).

## Deliverables

- prototype3.md (this document)
- include/heap.h
- src/heap.c
- Updated Makefile to compile heap.c
- Updated kernel init sequence to call heap_init() and run tests

## Next Prototype (Planned)

Prototype 4: Timer interrupts
- Enable interrupts (sti)
- Configure PIT+PIC (teaching-simple) or APIC timer (modern)
- Install IRQ handler
- Maintain tick counter
- Print periodic status using throttling
```
