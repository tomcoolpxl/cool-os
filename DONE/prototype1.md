Prototype 1 goal: make the kernel fault-tolerant and debuggable by installing a real IDT and exception handlers that print a usable crash report to serial (and then halt). No timers, no keyboard, no APIC yet.

## Prototype 1 requirements

### Functional requirements

1. IDT installed (256 entries)

* Use interrupt gates (type 0x8E)
* Present bit set
* Selector = kernel code segment
* DPL = 0

2. Exception coverage (minimum)

* #DF (vector 8) double fault
* #GP (vector 13) general protection fault
* #PF (vector 14) page fault
* One catch-all handler for all other vectors (at least for exceptions 0-31)

3. Crash report to serial
   On any exception print at least:

* vector number
* error code (0 if none)
* RIP, CS, RFLAGS, RSP, SS
* CR2 for page faults
* optional but strongly recommended: RAX..R15

Then halt in a loop.

4. Enable interrupts policy

* Do NOT execute sti in prototype 1.
* Exceptions will still fire without sti (faults are synchronous).
* This keeps the state deterministic while you bring up IDT.

### Non-goals for Prototype 1

* No PIC/APIC programming
* No timer IRQ
* No keyboard IRQ
* No syscall entry
* No SMP
* No scheduling

## Design decisions

### Use assembly ISR stubs (recommended)

Do not use compiler interrupt attributes yet. Assembly stubs make:

* error-code vs no-error-code vectors explicit
* register saving deterministic
* stack frame layout stable across compiler versions

### Error code normalization

CPU pushes an error code only for specific exceptions (e.g., #DF, #GP, #PF). For others, it does not.

Your stubs should normalize this by always calling a common C handler with:

* vector number
* error code (0 for no-error-code exceptions)
* pointer to a saved register/context struct

This avoids duplicating C-side logic.

### Double fault robustness (two options)

Option A (minimal): handle #DF on current stack

* simplest
* acceptable for teaching if your stack is reliable

Option B (better): set up IST for #DF

* requires TSS and a GDT TSS descriptor
* adds complexity now but prevents "fault while faulting" from becoming a triple fault
* recommended if you want a stable debugging experience when you later break paging

If you want the cleanest teaching arc, do Option A in Proto 1 and add IST in Proto 1.1 before paging work.

## Proposed file/module layout

Add these files:

* include/idt.h

  * idt_init()
  * idt_set_gate(vector, isr_addr, flags)

* src/idt.c

  * builds IDT table
  * loads it with lidt

* src/isr.S

  * 256 (or at least 32) ISR stubs
  * common stub that saves registers and calls C handler

* include/isr.h

  * struct for saved registers
  * isr_common_handler(...) declaration

* src/isr.c

  * isr_common_handler implementation
  * prints crash report via serial + panic halt

* include/cpu.h (or similar)

  * read_cr2()
  * read_rflags() (optional)
  * lidt wrapper (or do it in idt.c)

## IDT data structures (must be correct)

IDT entry (x86-64) fields:

* offset bits 0..15
* selector
* IST (3 bits)
* type/attr
* offset bits 16..31
* offset bits 32..63
* zero

IDTR:

* limit = sizeof(idt) - 1
* base = address of idt

## Exception testing plan (must be scripted)

Add a Make target to run and immediately trigger controlled faults, one at a time, for validation.

In kernel.c after idt_init(), call one of these based on a compile-time define or a constant:

1. #UD invalid opcode

* asm volatile ("ud2");

2. #GP (reliable method)

* load an invalid segment selector, or execute privileged instruction from wrong context (less reliable in ring 0)
* easiest for Proto 1 is #UD; #GP can be triggered by invalid IDT gate or invalid stack, but that is too indirect.
* Alternative: deliberately lidt a misaligned IDT pointer after first success to see #GP (not recommended).
  Practical for teaching: prioritize #PF and #UD first; then add an intentional #GP once you add minimal segment helpers.

3. #PF page fault

* volatile uint64_t *p = (uint64_t*)0xdeadbeefdeadbeef; *p = 1;

Expected output for #PF:

* vector 14
* CR2 equals the faulting address
* error code decodes make sense (present/write/user bits)

Also add QEMU flags already present:

* -no-reboot
* -no-shutdown (recommended) so QEMU stays open after halt
* -serial stdio

Acceptance criteria:

* Each fault prints a report and halts
* No reboot, no silent hang, no triple fault

## Implementation sequence (lowest risk)

1. Implement IDT structures and lidt

* Install gates for vectors 0..31 pointing to temporary stubs that just halt (or print vector only)

2. Implement ISR stubs for vectors 0..31

* Two macros: with_error_code, without_error_code

3. Implement common handler in C

* Print vector, error code, RIP/RSP/RFLAGS, CR2 if vector==14
* Print GPRs if you saved them

4. Wire up kernel init flow

* serial_init()
* hhdm_init()
* idt_init()
* trigger a deliberate fault for test

5. Once stable, optionally fill all 256 IDT entries with a default handler

* This prevents unexpected vectors from jumping to null.

## Deliverable: prototype1.md (what it should claim)

* IDT installed
* Exceptions #PF/#GP/#DF handled
* Crash report prints registers + CR2
* Deterministic behavior under QEMU with -no-reboot/-no-shutdown

If you want, paste your current repo tree and I will propose exact filenames, interfaces, and the minimal set of stubs (32 vectors vs full 256) that keeps Proto 1 small but not fragile.
