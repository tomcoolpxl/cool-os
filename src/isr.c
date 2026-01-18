#include "isr.h"
#include "cpu.h"
#include "serial.h"
#include "task.h"
#include "scheduler.h"

/* Re-entrancy guard to prevent recursive exceptions during crash report */
static volatile int in_handler = 0;

/* Exception names for vectors 0-31 */
static const char *exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug",
    "NMI Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 Floating-Point Exception",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV Hypervisor Injection Exception",
    "#VC VMM Communication Exception",
    "#SX Security Exception",
    "Reserved"
};

static void print_hex64(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static void print_hex32(uint32_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static void print_reg(const char *name, uint64_t val) {
    serial_puts(name);
    serial_puts(": ");
    print_hex64(val);
    serial_puts("\n");
}

/* Decode page fault error code bits */
static void print_pf_error(uint64_t error_code) {
    serial_puts("  Page fault: ");
    serial_puts((error_code & 0x01) ? "protection violation" : "page not present");
    serial_puts(", ");
    serial_puts((error_code & 0x02) ? "write" : "read");
    serial_puts(", ");
    serial_puts((error_code & 0x04) ? "user mode" : "supervisor mode");
    if (error_code & 0x08) serial_puts(", reserved bit set");
    if (error_code & 0x10) serial_puts(", instruction fetch");
    if (error_code & 0x20) serial_puts(", protection key");
    if (error_code & 0x40) serial_puts(", shadow stack");
    if (error_code & 0x8000) serial_puts(", SGX");
    serial_puts("\n");
}

void isr_handler(struct interrupt_frame *frame) {
    /* Disable interrupts (should already be disabled by interrupt gate) */
    asm volatile("cli");

    /*
     * Check if fault came from user mode (RPL of CS is 3).
     * If so, kill the task and yield instead of crashing the kernel.
     */
    int from_user = (frame->cs & 3) == 3;
    if (from_user) {
        serial_puts("USER FAULT: Task ");
        /* Print task ID as hex digit */
        task_t *t = task_current();
        if (t) {
            serial_putc('0' + (char)(t->id % 10));
        }
        serial_puts(" killed (");
        if (frame->vector < 32) {
            /* Print just the exception name */
            if (frame->vector == 6) serial_puts("#UD");
            else if (frame->vector == 13) serial_puts("#GP");
            else if (frame->vector == 14) serial_puts("#PF");
            else serial_puts("exception");
        } else {
            serial_puts("exception");
        }
        serial_puts(") at RIP ");
        print_hex64(frame->rip);
        serial_puts("\n");

        /* Mark task as finished and yield to let scheduler pick next task */
        if (t) {
            t->state = TASK_FINISHED;
        }
        scheduler_yield();
        /* Should not return, but just in case */
        return;
    }

    /* Re-entrancy guard: if we fault while handling a fault, halt immediately */
    if (in_handler) {
        serial_puts("\n!!! NESTED EXCEPTION - HALTING !!!\n");
        cpu_halt();
    }
    in_handler = 1;

    serial_puts("\n========== EXCEPTION ==========\n");

    /* Exception name and vector */
    if (frame->vector < 32) {
        serial_puts(exception_names[frame->vector]);
    } else {
        serial_puts("Unknown Exception");
    }
    serial_puts(" (vector ");
    print_hex32((uint32_t)frame->vector);
    serial_puts(")\n");

    /* Error code */
    serial_puts("Error code: ");
    print_hex64(frame->error_code);
    serial_puts("\n");

    /* For page faults, show CR2 and decoded error bits */
    if (frame->vector == 14) {
        serial_puts("CR2 (fault address): ");
        print_hex64(read_cr2());
        serial_puts("\n");
        print_pf_error(frame->error_code);
    }

    serial_puts("\n--- CPU State ---\n");

    /* Instruction pointer and code segment */
    print_reg("RIP   ", frame->rip);
    print_reg("CS    ", frame->cs);
    print_reg("RFLAGS", frame->rflags);
    print_reg("RSP   ", frame->rsp);
    print_reg("SS    ", frame->ss);

    serial_puts("\n--- General Purpose Registers ---\n");

    print_reg("RAX", frame->rax);
    print_reg("RBX", frame->rbx);
    print_reg("RCX", frame->rcx);
    print_reg("RDX", frame->rdx);
    print_reg("RSI", frame->rsi);
    print_reg("RDI", frame->rdi);
    print_reg("RBP", frame->rbp);
    print_reg("R8 ", frame->r8);
    print_reg("R9 ", frame->r9);
    print_reg("R10", frame->r10);
    print_reg("R11", frame->r11);
    print_reg("R12", frame->r12);
    print_reg("R13", frame->r13);
    print_reg("R14", frame->r14);
    print_reg("R15", frame->r15);

    serial_puts("\n--- Control Registers ---\n");
    print_reg("CR2", read_cr2());
    print_reg("CR3", read_cr3());

    serial_puts("\n========== SYSTEM HALTED ==========\n");

    cpu_halt();
}
