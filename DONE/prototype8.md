Two things:

1. Your Prototype 7 is functionally impressive (SYSCALL/SYSRET, TSS.RSP0 switching, user task yield, user fault isolation).
2. Your fault log strongly suggests a serious correctness issue: your “user” code is still executing at a kernel virtual address.

You printed:

`USER FAULT ... (#UD) at RIP 0xffffffff80001010`

That address is in the typical higher-half kernel range. In a sane design, ring 3 RIP should be in a user range like `0x0000000000400000` (or any low canonical user region), not `0xffffffff...`.

That usually means one of these is true:

* You marked kernel pages as user-accessible (U/S bit = 1) and then jumped into kernel text from ring 3.
* Or your user trampoline/test function is linked into the kernel image and you are entering ring 3 without relocating code into a user region.
* Or your page table “U/S bit manipulation” is too broad (eg setting U on all mappings).

Before Prototype 8, fix that. Otherwise the next steps (ELF loader, libc, doom path) will be built on a broken privilege model.

What you want after Prototype 7:

* Kernel higher-half mappings: U/S = 0 (supervisor only), NX appropriately
* User text/data/stack mappings in low half: U/S = 1 (user), with NX set for data/stack
* Ring 3 code must never have RIP in kernel half.

Now, here is Prototype 8 as a complete markdown spec, aligned with your current architecture (SYSCALL/SYSRET, per-task kernel/user stack, single address space acceptable initially).

```md
# Prototype 8: ELF64 Loader and Real User Programs

## Purpose

Replace hardcoded user entry functions with loading and executing real ELF64 user programs. This prototype establishes the user program ABI surface needed for a minimal libc and a shell later.

Prototype 8 must run user code in ring 3 from a user address range, not from the kernel higher-half mapping.

## Scope

In scope:
- Load ELF64 executables into a user virtual address range
- Map segments with correct page permissions (U/S, NX, W)
- Create user stack and pass initial process state
- Start user program in ring 3 (via SYSRET or IRETQ path already implemented)
- Minimal "init" user program loaded from a Limine module (recommended first)
- Validation tests for correct privilege separation

Out of scope:
- Filesystem-based program loading (FAT/VFS) (later prototype)
- Dynamic linking (ELF interpreter)
- Preemptive scheduling
- Copy-on-write fork
- ASLR
- Demand paging

## Dependencies

Required components:
- Prototype 7: user mode tasks, SYSCALL/SYSRET entry, per-task kernel/user stacks, syscalls (exit, write, yield)
- Working paging manipulation with per-page U/S and NX control
- Kernel heap
- Exception handling that can kill a user task without killing the kernel

Recommended:
- Limine module request support (LIMINE_EXECUTABLE_AND_MODULES_REQUEST)
- HHDM helpers

## Address Space Model

### Kernel mappings
- Kernel is mapped in higher half (example: 0xFFFFFFFF80000000)
- All kernel pages MUST have U/S bit cleared (supervisor only)
- Kernel text should be RX, kernel data RW, NX as appropriate

### User mappings
- User programs are mapped into low canonical addresses, example:
  - User image base: 0x0000000000400000
  - User stack top:  0x0000000070000000 (example)
- User pages MUST have U/S bit set (user accessible)
- User text: RX (W=0, NX=0)
- User rodata: R (NX=0)
- User data/bss: RW (NX=1 recommended)
- User stack: RW (NX=1)

Single address space is allowed for Prototype 8, but privilege separation MUST be enforced by page U/S bits.

## ELF Loader Requirements

### Supported ELF subset
- ELF64, little-endian, x86-64
- Type: ET_EXEC (preferred) or ET_DYN with fixed mapping (optional later)
- Program headers: PT_LOAD only
- Ignore section headers for loading

### Validation checks
Loader must reject binaries if:
- Not ELF64
- Not x86-64 machine type
- No PT_LOAD segments
- Segment virtual addresses not in allowed user range
- Segment sizes invalid (memsz < filesz)
- Overlaps forbidden ranges (kernel region, null page if you keep it unmapped)

### Mapping behavior
For each PT_LOAD:
- Align down vaddr to PAGE_SIZE boundary
- Map pages for [vaddr, vaddr + memsz) with correct flags
- Copy file bytes into mapped memory for [vaddr, vaddr + filesz)
- Zero-fill [vaddr + filesz, vaddr + memsz)

Mapping flags derived from p_flags:
- PF_R: readable (implied)
- PF_W: writable => W=1
- PF_X: executable => NX=0, else NX=1
Always set U/S=1 for user segments.

### Entry point
- Use ELF header e_entry as initial RIP
- Must be in user mapped range

## Source of Executable (Phase 1)

To avoid needing a filesystem now, load the user program from a Limine module:

- Add a Limine executable_and_modules request
- Include a module file (init.elf) in your ESP image build
- Kernel finds module by name and passes its in-memory bytes to ELF loader

This keeps Prototype 8 focused on ELF and paging, not disk IO.

## User Stack Setup

Allocate user stack pages (example: 4 pages = 16 KiB) and map them:

- Stack top is a chosen user address (aligned)
- Set task->user_rsp to stack_top - initial_frame_size

Initial content minimal for Prototype 8:
- Push a fake return address = 0
- Optionally push argc/argv pointers later

No full System V process ABI required yet, but ensure stack alignment:
- RSP must be 16-byte aligned at user entry

## Task Integration

Provide API:

- task_t *task_create_elf_module(const char *module_name);

Behavior:
- Finds module bytes
- Loads ELF into user range
- Creates per-task kernel stack and user stack
- Sets user_rip to ELF entry
- Sets user_rsp appropriately
- Marks task as is_user = 1
- Schedules it

When user program calls SYS_exit:
- Task is marked FINISHED and removed

## Validation Tests

### TEST1: Run init user ELF that prints and exits
- init.elf calls SYS_write("hello") then SYS_exit(0)
Expected:
- Output printed
- Task exits cleanly
- Kernel remains in idle

### TEST2: Two ELF user programs yielding
- u1.elf and u2.elf alternate via SYS_yield
Expected:
- Alternating output

### TEST3: Privilege separation sanity
Attempt from user mode:
- Execute instruction that requires supervisor page access by jumping to a kernel address
Expected:
- Page fault or GP fault in user mode
- Task killed
- Kernel survives

Critical acceptance check:
- Any user fault RIP must be in user range, not 0xffffffff....

## Acceptance Criteria

Prototype 8 is complete when:
- Kernel loads at least one ELF64 user program from a Limine module
- Program runs in ring 3 at a user virtual address
- User program uses syscalls successfully (write, exit, yield)
- Kernel pages are not user-accessible (U/S=0)
- User attempts to access kernel memory triggers user fault and kills task
- System remains stable for repeated runs

## Deliverables

- prototype8.md (this document)
- elf loader implementation (include/elf.h, src/elf.c or similar)
- module lookup support (if not already present)
- user stack mapping code
- updated kernel tests and ESP build to include init.elf module

## Next Prototype (Planned)

Prototype 9: Filesystem-backed program loading
- FAT read + seek
- Load ELF from disk path rather than Limine module
- Basic VFS interface
```

If you want the next step to be “as small as possible”, implement Prototype 8 using a Limine module (init.elf) first. That avoids filesystem work and lets you validate the single most important thing right now: user code actually runs from a user address range and cannot execute kernel text.

If you paste (or summarize) how you currently set the U/S bit in paging.c (what ranges you mark user), I can tell you exactly why RIP ended up in the kernel higher half and what change will fix it.
