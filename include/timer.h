#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "isr.h"

/* Timer frequency in Hz (configured via pit_init) */
#define TIMER_HZ 100

/* Initialize the timer subsystem */
void timer_init(void);

/* Get current tick count */
uint64_t timer_get_ticks(void);

/* Sleep for specified number of ticks */
void timer_sleep_ticks(uint64_t ticks);

/* Sleep for specified number of milliseconds */
void timer_sleep_ms(uint64_t ms);

#endif
