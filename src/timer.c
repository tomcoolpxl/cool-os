#include "timer.h"
#include "pit.h"
#include "pic.h"
#include "isr.h"
#include "kbd.h"
#include "xhci.h"

#define IRQ_TIMER    0x20
#define IRQ_KEYBOARD 0x21
#define IRQ_XHCI     0x22

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
    } else if (frame->vector == IRQ_KEYBOARD) {
        kbd_handle_irq();
        pic_send_eoi(1);  /* IRQ1 = keyboard */
    } else if (frame->vector == IRQ_XHCI) {
        xhci_handle_irq();
        /* No PIC EOI needed for MSI, but good to know it fired */
    }
}
