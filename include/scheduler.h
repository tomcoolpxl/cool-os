#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "isr.h"

/* Preemptive scheduling constants (Proto 17) */
#define SCHED_TICK_SLICE 5  /* Time slice in ticks (50ms at 100Hz) */

void scheduler_init(void);
void scheduler_add(task_t *task);
void scheduler_yield(void);

/*
 * Preemptive scheduler entry point - called from timer IRQ handler.
 * Saves current task's RSP from interrupt frame, selects next task,
 * and sets preempt_new_rsp for the IRQ stub to perform the switch.
 */
void scheduler_preempt(struct interrupt_frame *frame);

#endif
