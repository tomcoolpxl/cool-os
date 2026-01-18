#include <stdint.h>
#include <stddef.h>
#include "task.h"
#include "scheduler.h"
#include "pmm.h"
#include "heap.h"
#include "hhdm.h"
#include "panic.h"

static uint64_t next_task_id = 0;

/* Current task pointer - exported for scheduler */
task_t *current_task = NULL;

/* Forward declaration */
static void task_trampoline(void);

task_t *task_create(void (*entry)(void)) {
    /* Allocate task struct from heap */
    task_t *task = kmalloc(sizeof(task_t));
    ASSERT(task != NULL);

    /* Allocate stack from PMM (one frame = 4 KiB) */
    uint64_t stack_phys = pmm_alloc_frame();
    ASSERT(stack_phys != 0);
    void *stack_base = (void *)phys_to_hhdm(stack_phys);

    task->stack_base = stack_base;
    task->entry = entry;
    task->state = TASK_READY;
    task->id = next_task_id++;
    task->next = NULL;

    /*
     * Set up initial stack frame for context_switch.
     * Stack grows downward, so start at top of allocated region.
     * We push: trampoline return address + 6 callee-saved registers (zeros)
     * so that first context_switch pops zeros and "returns" to trampoline.
     */
    uint64_t *sp = (uint64_t *)((uint8_t *)stack_base + TASK_STACK_SIZE);

    /* Return address - where context_switch's ret will jump */
    *(--sp) = (uint64_t)task_trampoline;

    /* Callee-saved registers (pushed in order: rbp, rbx, r12, r13, r14, r15) */
    /* We pop in reverse order, so push: r15, r14, r13, r12, rbx, rbp */
    *(--sp) = 0;  /* r15 */
    *(--sp) = 0;  /* r14 */
    *(--sp) = 0;  /* r13 */
    *(--sp) = 0;  /* r12 */
    *(--sp) = 0;  /* rbx */
    *(--sp) = 0;  /* rbp */

    task->rsp = (uint64_t)sp;

    return task;
}

/*
 * Trampoline function: called when a new task starts executing.
 * Calls the task's entry point, then marks task as finished and yields.
 */
static void task_trampoline(void) {
    /* Enable interrupts - new task starts with interrupts disabled from scheduler_yield */
    asm volatile("sti");

    /* Call the actual entry function */
    current_task->entry();

    /* Task has returned - mark as finished and yield */
    current_task->state = TASK_FINISHED;
    task_yield();

    /* Should never reach here */
    ASSERT(0);
}

void task_yield(void) {
    scheduler_yield();
}

task_t *task_current(void) {
    return current_task;
}
