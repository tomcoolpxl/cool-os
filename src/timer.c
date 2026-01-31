#include "timer.h"
#include "pit.h"
#include "pic.h"
#include "isr.h"
#include "kbd.h"
#include "xhci.h"
#include "serial.h"
#include "task.h"
#include "scheduler.h"

#define IRQ_TIMER    0x20
#define IRQ_KEYBOARD 0x21
#define IRQ_XHCI     0x22
#define IRQ_XHCI_MSI 0x40

/* External: current_task is defined in task.c */
extern task_t *current_task;

void timer_init(void) {
    /* Timer is already set up by pic_init() and pit_init() */
    /* This function exists for future expansion (e.g., callback registration) */
}

uint64_t timer_get_ticks(void) {
    return pit_get_ticks();
}

void timer_sleep_ticks(uint64_t ticks) {
    uint64_t target = timer_get_ticks() + ticks;
    while (timer_get_ticks() < target) {
        asm volatile("hlt");
    }
}

void timer_sleep_ms(uint64_t ms) {
    /* Round up to ensure minimum delay is met */
    uint64_t ticks = (ms * TIMER_HZ + 999) / 1000;
    timer_sleep_ticks(ticks);
}

/*
 * IRQ handler called from assembly stub.
 * Dispatches to appropriate handler based on vector number.
 */
void irq_handler(struct interrupt_frame *frame) {
    if (frame->vector == IRQ_TIMER) {
        pit_tick();
        pic_send_eoi(0);  /* IRQ0 = timer */

        /*
         * Preemptive scheduling (Proto 17).
         * Decrement time slice for current task and preempt if expired.
         * Only preempt RUNNING tasks - idle, zombie, blocked tasks skip.
         */
        if (current_task != NULL && current_task->state == PROC_RUNNING) {
            if (current_task->ticks_remaining > 0) {
                current_task->ticks_remaining--;
            }
            if (current_task->ticks_remaining == 0) {
                scheduler_preempt(frame);
            }
        }
    } else if (frame->vector == IRQ_KEYBOARD) {
        kbd_handle_irq();
        pic_send_eoi(1);  /* IRQ1 = keyboard */
    } else if (frame->vector == IRQ_XHCI || frame->vector == IRQ_XHCI_MSI) {
        xhci_handle_irq();
        /* No PIC EOI needed for MSI, but good to know it fired */
    }
}
