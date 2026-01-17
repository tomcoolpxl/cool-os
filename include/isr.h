#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * Interrupt frame as pushed by ISR stubs and CPU.
 * This matches the stack layout after isr_common_stub saves all registers.
 */
struct interrupt_frame {
    /* Pushed by stub (in reverse order of push instructions) */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    /* Pushed by stub before GPRs */
    uint64_t vector;
    uint64_t error_code;
    /* Pushed by CPU on interrupt/exception */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

/* C handler called from assembly stub (exceptions) */
void isr_handler(struct interrupt_frame *frame);

/* C handler called from assembly stub (IRQs) */
void irq_handler(struct interrupt_frame *frame);

/* Assembly stub table (defined in isr_stubs.S) */
extern void *isr_stub_table[32];

/* Default stub for vectors 32-255 */
extern void isr_stub_default(void);

/* IRQ stub for timer (vector 0x20) */
extern void irq_stub_0x20(void);

#endif
