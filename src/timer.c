#include "timer.h"
#include "pit.h"
#include "pic.h"
#include "serial.h"
#include "isr.h"

#define IRQ_TIMER  0x20

void timer_init(void) {
    /* Timer is already set up by pic_init() and pit_init() */
    /* This function exists for future expansion (e.g., callback registration) */
}

/*
 * IRQ handler called from assembly stub.
 * Currently hardwired to handle timer (vector 0x20) only.
 */
void irq_handler(struct interrupt_frame *frame) {
    if (frame->vector == IRQ_TIMER) {
        pit_tick();

        /* Print tick count every 100 ticks (once per second at 100 Hz) */
        uint64_t ticks = pit_get_ticks();
        if (ticks % 100 == 0) {
            serial_puts("tick=");
            /* Simple decimal print for tick count */
            char buf[21];  /* max uint64_t is 20 digits + null */
            int i = 20;
            buf[i] = '\0';
            uint64_t val = ticks;
            do {
                buf[--i] = '0' + (val % 10);
                val /= 10;
            } while (val > 0);
            serial_puts(&buf[i]);
            serial_puts("\n");
        }

        /* Send end-of-interrupt to PIC */
        pic_send_eoi(0);  /* IRQ0 = timer */
    }
}
