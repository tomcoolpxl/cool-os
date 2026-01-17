#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/*
 * 8253/8254 Programmable Interval Timer (PIT) driver
 *
 * The PIT runs at a base frequency of 1.193182 MHz.
 * We program channel 0 to generate interrupts at a specified rate.
 */

#define PIT_FREQ  1193182  /* Base oscillator frequency in Hz */

/* Initialize PIT channel 0 at the specified frequency (Hz) */
void pit_init(uint32_t hz);

/* Get current tick count since PIT initialization */
uint64_t pit_get_ticks(void);

/* Increment tick count (called from timer interrupt handler) */
void pit_tick(void);

#endif
