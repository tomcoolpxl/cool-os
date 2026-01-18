/*
 * SYSCALL entry point for x86-64
 *
 * On SYSCALL entry:
 *   RCX = user RIP (return address)
 *   R11 = user RFLAGS
 *   RSP = user stack pointer (unchanged!)
 *   RAX = syscall number
 *   RDI, RSI, RDX = syscall arguments (per System V ABI)
 *
 * Task structure offsets (must match task.h):
 *   offset 0:  rsp (kernel RSP saved by context_switch)
 *   offset 56: kernel_rsp (top of kernel stack for syscalls)
 */

.code64
.global syscall_entry

/* Scratch space for saving user RSP during syscall */
.section .data
user_rsp_scratch:
    .quad 0

.section .text
syscall_entry:
    /*
     * Interrupts are disabled by FMASK clearing IF.
     * We're still on user stack - need to switch to kernel stack ASAP.
     */

    /* Save user RSP to scratch space (can't push yet - still on user stack) */
    movq %rsp, user_rsp_scratch(%rip)

    /* Load kernel stack from current_task->kernel_rsp (offset 56) */
    movq current_task(%rip), %rsp
    movq 56(%rsp), %rsp

    /* Now on kernel stack - save user context */
    pushq user_rsp_scratch(%rip)   /* User RSP */
    pushq %r11                      /* User RFLAGS */
    pushq %rcx                      /* User RIP */

    /* Save caller-saved registers that we might clobber */
    pushq %rax
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10

    /* Re-enable interrupts now that we're safely in kernel mode */
    sti

    /*
     * Call syscall_dispatch(num, arg1, arg2, arg3)
     * Arguments are already in the right registers:
     *   RAX = syscall number -> move to RDI (first arg)
     *   RDI = arg1 -> move to RSI (second arg)
     *   RSI = arg2 -> move to RDX (third arg)
     *   RDX = arg3 -> move to RCX (fourth arg)
     */
    movq %rdx, %rcx         /* arg3 -> RCX */
    movq %rsi, %rdx         /* arg2 -> RDX */
    movq %rdi, %rsi         /* arg1 -> RSI */
    movq %rax, %rdi         /* num -> RDI */

    /* Ensure 16-byte stack alignment before call */
    /* Stack has 10 pushes (80 bytes) - need to align */
    subq $8, %rsp
    call syscall_dispatch
    addq $8, %rsp

    /* Return value is in RAX - save it temporarily */
    movq %rax, %r10

    /* Disable interrupts before returning to user mode */
    cli

    /* Restore caller-saved registers */
    popq %r10               /* Discard, we have return value */
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    addq $8, %rsp           /* Skip saved RAX, use return value instead */

    /* Restore user context for SYSRET */
    popq %rcx               /* User RIP */
    popq %r11               /* User RFLAGS */
    popq %rsp               /* User RSP */

    /* Return to user mode
     * SYSRET loads:
     *   RIP from RCX
     *   RFLAGS from R11 (with RF and VM cleared, bits 1,3,5,9 set)
     *   CS from STAR[63:48] + 16 (with RPL forced to 3)
     *   SS from STAR[63:48] + 8 (with RPL forced to 3)
     */
    sysretq
