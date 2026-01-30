````md
# Prototype 25: mmap(), Memory-Mapped Files, and Shared Memory Regions

## Purpose

Introduce memory-mapped I/O via mmap(). This allows files and anonymous memory to be mapped directly into a process address space, enabling efficient file access, shared memory between processes, and advanced application memory models.

After this prototype:

- User programs can map files into memory
- Anonymous shared memory is possible
- Page fault handler participates in on-demand mapping
- Foundation for shared libraries and IPC is established

## Scope

In scope:
- mmap() syscall (anonymous + file-backed)
- munmap() syscall
- Virtual memory region (VMR) tracking
- Page fault driven mapping for file-backed pages
- Shared anonymous memory mappings
- Read-only and read-write mappings

Out of scope:
- PROT_EXEC mappings
- MAP_PRIVATE copy-on-write semantics (optional later)
- Swapping
- Memory overcommit
- Huge pages
- NUMA

## Dependencies

Required components:
- Per-process address spaces (Prototype 16)
- fork() and COW framework (Prototype 18)
- execve() and ELF loading (Prototype 19)
- Preemptive scheduler (Prototype 17)
- FD table and file IO (Prototype 21)
- VFS layer
- Page fault handler with error code decoding

## mmap() API

Userland:

```c
void *mmap(void *addr,
           size_t length,
           int prot,
           int flags,
           int fd,
           size_t offset);
````

Supported flags (Prototype 25 subset):

* MAP_ANONYMOUS
* MAP_SHARED
* MAP_FIXED (optional, may reject initially)

Supported prot flags:

* PROT_READ
* PROT_WRITE

Return:

* Virtual address of mapping
* MAP_FAILED ((void *)-1) on error

## munmap() API

Userland:

```c
int munmap(void *addr, size_t length);
```

Behavior:

* Unmap region
* Release physical frames
* Remove region from VMR list

## Virtual Memory Region (VMR) Tracking

Each process maintains a list of mapped regions:

```c
typedef struct vm_region {
    uint64_t start;
    uint64_t end;

    int prot;
    int flags;

    int fd;          // -1 for anonymous
    size_t offset;

    struct vm_region *next;
} vm_region_t;
```

This structure is used by the page fault handler.

## Address Space Layout

Reserve user mapping area:

Example:

* Heap end: dynamic
* mmap base: 0x0000000080000000
* mmap grows upward

Avoid collisions with:

* Stack
* Heap
* ELF segments

Kernel must track free virtual ranges.

## Mapping Types

### Anonymous Shared Mapping

Flags:

* MAP_ANONYMOUS | MAP_SHARED

Behavior:

* Allocate physical pages on first page fault
* Map into process with requested permissions
* Share mapping across fork()

### File-Backed Mapping

Flags:

* MAP_SHARED

Behavior:

* On page fault:

  * Read corresponding file page via VFS
  * Map into process
* Multiple processes mapping same file+offset share frames

Writable file-backed mappings optional:

* Read-only mapping required first
* Write support may be added later

## Page Fault Integration

Extend page fault handler:

When fault address matches a VMR:

If anonymous:

* Allocate frame
* Zero fill
* Map page

If file-backed:

* Allocate frame
* Read file block(s)
* Copy into frame
* Map page

If protection violation:

* Kill process

If no matching VMR:

* Normal fault handling

## Reference Counting

For MAP_SHARED:

* Physical frames must use refcounts
* fork() inherits mappings
* munmap decrements refcounts
* Frame freed when refcount == 0

Reuse COW refcount infrastructure.

## Kernel File Cache (Optional)

Optional simple cache:

* Cache recently mapped file pages
* Avoid repeated disk reads

Not required for correctness.

## Shell and Userland Integration

Shell does not need to expose mmap directly.

Provide test user programs to verify:

* File mapping
* Anonymous mapping
* Shared memory behavior

## Validation Tests

### TEST1: Anonymous Mapping

User program:

* mmap anonymous region
* Write pattern
* Read back
* Print result

Expected:

* Correct data
* No fault

### TEST2: File Mapping Read

User program:

* mmap("TEST.TXT")
* Print first bytes

Expected:

* Matches file contents

### TEST3: Shared Mapping Fork

User program:

* mmap anonymous MAP_SHARED
* fork()
* Child writes
* Parent reads

Expected:

* Parent sees child's change

### TEST4: Unmap

User program:

* mmap region
* munmap region
* Access region

Expected:

* Page fault and process termination

## Safety Requirements

* Validate user address ranges
* Prevent kernel address mapping
* Enforce permissions correctly
* Avoid mapping overlap
* Prevent memory leaks
* Correct refcount handling

## Implementation Notes

### Suggested Files

New:

* include/mmap.h
* src/mmap.c
* include/vm_region.h

Modified:

* src/syscall.c
* src/paging.c
* src/task.c
* src/isr.c (page fault handler)
* src/vfs.c

### Fault Handling Order

Page fault handler priority:

1. Check kernel faults
2. Check mmap regions
3. Check COW faults
4. Default kill

### Alignment

All mappings:

* Page-aligned start
* Length rounded up to page size

## Acceptance Criteria

Prototype 25 is complete when:

* mmap and munmap syscalls work
* Anonymous and file-backed mappings function
* Shared mappings survive fork()
* Page faults map pages correctly
* No crashes or leaks under repeated mapping use

## Next Prototype (Planned)

Prototype 26: Dynamic Linking and Shared Libraries (Optional Track)

Goals:

* ELF ET_DYN support
* Shared library mapping via mmap
* Runtime loader
* libc as shared object

```
```
