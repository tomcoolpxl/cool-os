#include "timer.h"
#include "pit.h"
#include "pic.h"
#include "isr.h"

#define IRQ_TIMER  0x20

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
 * Currently hardwired to handle timer (vector 0x20) only.
 */
void irq_handler(struct interrupt_frame *frame) {
    if (frame->vector == IRQ_TIMER) {
        pit_tick();
        pic_send_eoi(0);  /* IRQ0 = timer */
    }
}
