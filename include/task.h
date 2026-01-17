#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_RUNNING   0
#define TASK_READY     1
#define TASK_FINISHED  2

#define TASK_STACK_SIZE  4096  /* 4 KiB per task (1 PMM frame) */

typedef struct task {
    uint64_t rsp;           /* Saved stack pointer - MUST be at offset 0 */
    struct task *next;      /* Next in scheduler queue (circular) */
    int state;              /* TASK_RUNNING/READY/FINISHED */
    void *stack_base;       /* Stack allocation base */
    uint64_t id;            /* Task ID for debugging */
    void (*entry)(void);    /* Entry point function */
} task_t;

task_t *task_create(void (*entry)(void));
void task_yield(void);
task_t *task_current(void);

#endif
