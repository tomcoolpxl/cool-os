/*
 * ISR assembly stubs for x86-64 exception vectors 0-31
 *
 * Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
 * All others need a dummy error code pushed.
 */

.code64

/* Macro for exceptions WITHOUT error code (push dummy 0) */
.macro ISR_NOERR vector
.global isr_stub_\vector
isr_stub_\vector:
    pushq $0                    /* dummy error code */
    pushq $\vector              /* vector number */
    jmp isr_common_stub
.endm

/* Macro for exceptions WITH error code (CPU already pushed it) */
.macro ISR_ERR vector
.global isr_stub_\vector
isr_stub_\vector:
    pushq $\vector              /* vector number */
    jmp isr_common_stub
.endm

/* Generate stubs for vectors 0-31 */
ISR_NOERR 0     /* #DE - Divide Error */
ISR_NOERR 1     /* #DB - Debug */
ISR_NOERR 2     /* NMI - Non-Maskable Interrupt */
ISR_NOERR 3     /* #BP - Breakpoint */
ISR_NOERR 4     /* #OF - Overflow */
ISR_NOERR 5     /* #BR - Bound Range Exceeded */
ISR_NOERR 6     /* #UD - Invalid Opcode */
ISR_NOERR 7     /* #NM - Device Not Available */
ISR_ERR   8     /* #DF - Double Fault (error code always 0) */
ISR_NOERR 9     /* Coprocessor Segment Overrun (reserved) */
ISR_ERR   10    /* #TS - Invalid TSS */
ISR_ERR   11    /* #NP - Segment Not Present */
ISR_ERR   12    /* #SS - Stack-Segment Fault */
ISR_ERR   13    /* #GP - General Protection Fault */
ISR_ERR   14    /* #PF - Page Fault */
ISR_NOERR 15    /* Reserved */
ISR_NOERR 16    /* #MF - x87 Floating-Point Exception */
ISR_ERR   17    /* #AC - Alignment Check */
ISR_NOERR 18    /* #MC - Machine Check */
ISR_NOERR 19    /* #XM/#XF - SIMD Floating-Point Exception */
ISR_NOERR 20    /* #VE - Virtualization Exception */
ISR_ERR   21    /* #CP - Control Protection Exception */
ISR_NOERR 22    /* Reserved */
ISR_NOERR 23    /* Reserved */
ISR_NOERR 24    /* Reserved */
ISR_NOERR 25    /* Reserved */
ISR_NOERR 26    /* Reserved */
ISR_NOERR 27    /* Reserved */
ISR_NOERR 28    /* #HV - Hypervisor Injection Exception */
ISR_ERR   29    /* #VC - VMM Communication Exception */
ISR_ERR   30    /* #SX - Security Exception */
ISR_NOERR 31    /* Reserved */

/*
 * Common stub: save all GPRs, call C handler, halt
 *
 * Stack on entry:
 *   [rsp+0]  = vector
 *   [rsp+8]  = error_code (real or dummy)
 *   [rsp+16] = rip (pushed by CPU)
 *   [rsp+24] = cs
 *   [rsp+32] = rflags
 *   [rsp+40] = rsp (from before exception)
 *   [rsp+48] = ss
 */
isr_common_stub:
    /* Clear direction flag for string operations (ABI compliance) */
    cld

    /* Save all general-purpose registers */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Pass pointer to interrupt_frame as first argument */
    movq %rsp, %rdi

    /* Ensure 16-byte stack alignment for SysV ABI call */
    /* Frame is 22 * 8 = 176 bytes, already 16-byte aligned */

    /* Call C handler */
    call isr_handler

    /* Handler should not return, but halt if it does */
    cli
1:  hlt
    jmp 1b

/*
 * Default stub for vectors 32-255 (IRQs and software interrupts)
 * These should not fire in Proto 1, but having a handler prevents
 * jumping to null if something unexpected happens.
 */
.global isr_stub_default
isr_stub_default:
    pushq $0                    /* dummy error code */
    pushq $0xff                 /* vector 255 as "unknown" marker */
    jmp isr_common_stub

/*
 * IRQ stubs for hardware interrupts (vectors 0x20+)
 * Unlike exceptions, IRQs can return via iretq.
 */

/* Macro for IRQ stub (no error code from CPU) */
.macro IRQ_STUB vector
.global irq_stub_\vector
irq_stub_\vector:
    pushq $0                    /* dummy error code */
    pushq $\vector              /* vector number */
    jmp irq_common_stub
.endm

/* Generate IRQ stub for timer (vector 0x20 = 32) */
IRQ_STUB 0x20

/* Generate IRQ stub for keyboard (vector 0x21 = 33) */
IRQ_STUB 0x21

/* Generate IRQ stub for xHCI (vector 0x22 = 34) */
IRQ_STUB 0x22

/* Generate IRQ stub for xHCI (vector 0x40 = 64) */
IRQ_STUB 0x40

/*
 * Common IRQ stub: save all GPRs, call C handler, restore and iretq
 *
 * Stack on entry is identical to exception stub:
 *   [rsp+0]  = vector
 *   [rsp+8]  = error_code (dummy 0)
 *   [rsp+16] = rip (pushed by CPU)
 *   [rsp+24] = cs
 *   [rsp+32] = rflags
 *   [rsp+40] = rsp (from before interrupt)
 *   [rsp+48] = ss
 */
irq_common_stub:
    /* Clear direction flag for string operations (ABI compliance) */
    cld

    /* Save all general-purpose registers */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Pass pointer to interrupt_frame as first argument */
    movq %rsp, %rdi

    /* Call C handler */
    call irq_handler

    /* Restore all general-purpose registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* Pop vector and error_code */
    addq $16, %rsp

    /* Return from interrupt */
    iretq

/*
 * Table of ISR stub addresses for idt_init() to use
 */
.section .data
.global isr_stub_table
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
