1) Stack placement and alignment

Confirm in your kernel entry code:

Stack pointer is explicitly set or validated.

16-byte alignment before calling C code (SysV ABI requirement).

If Limine provides a stack and you rely on it:

Verify alignment with a debug print of RSP modulo 16.

Misaligned stack will break later when you add SSE instructions or compiler optimizations.

2) Paging assumptions with higher-half kernel

Your linker places kernel at:

0xFFFFFFFF80000000

Verify:

Limine higher-half mapping request is present and correct.

Kernel virtual address equals physical + offset mapping.

You are not accidentally identity-mapped only.

Test:

Print address of a global variable and compare against expected virtual range.

If this is wrong now, it will silently corrupt memory later.

3) Serial initialization ordering

Ensure:

UART init happens before first print.

No reliance on firmware-initialized UART state.

Many early kernels accidentally work only because OVMF left UART configured.

Force correctness:

Explicitly program baud divisor and line control registers every boot.

4) Limine protocol version pinning

Confirm:

Your limine.conf explicitly targets Limine 8.x protocol syntax.

Your limine.h header matches the Limine version you built.

Mismatch here can boot today and break after Limine update.

Recommendation:

Vendor Limine repo as submodule.

Pin commit hash.

5) QEMU run flags

Verify you are using:

-enable-kvm

-cpu host

Separate writable OVMF_VARS copy

If you reuse the same OVMF_VARS file repeatedly, NVRAM state can pollute testing.

Use:

cp OVMF_VARS.fd OVMF_VARS_WORK.fd

and boot with the copy.

Missing but recommended even for Proto 0

Not required to boot, but you should add now while codebase is small.

Add panic function

Implement:

panic(const char* msg)

Prints to serial

Halts CPU

Use this instead of infinite loops scattered in code.

Add early assert macro

Example behavior:

If condition fails

Print file:line

Halt

This becomes extremely valuable when adding paging and interrupts.

Proto 1 should be your next target

Do not jump to filesystem or graphics yet.

Your next minimal milestone should be:

Required additions

IDT setup

Exception handlers:

Page fault

General protection fault

Double fault

Timer interrupt (APIC or PIT)

sti enabling

Fault message printed to serial with register dump

Why this matters

Without exception handling:

Every bug becomes silent reset or freeze.

Debugging becomes guesswork.

Proto 1 transforms your kernel from "boots" to "debuggable".

Architecture check question (important)

Before going further:

Are you using Limine's higher-half direct map (HHDM) feature?
or

Are you doing manual offset mapping?

Your answer affects:

Memory allocator design

Physical memory access

DMA later

Answer this before implementing paging-related features.