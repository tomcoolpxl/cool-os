#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"
#include "task.h"
#include "heap.h"
#include "panic.h"
#include "serial.h"
#include "gdt.h"

/* External: current_task is defined in task.c */
extern task_t *current_task;

/* Assembly context switch function */
extern void context_switch(task_t *old, task_t *new);

/* Idle task: runs when no other tasks are ready */
static task_t *idle_task = NULL;

/* Idle task entry point: halts until next interrupt, then yields */
static void idle_entry(void) {
    for (;;) {
        asm volatile("sti; hlt");
        scheduler_yield();  /* Give other tasks a chance to run */
    }
}

void scheduler_init(void) {
    serial_puts("SCHED: Initializing scheduler\n");

    /*
     * Create bootstrap task to represent kmain's context.
     * This allows kmain to yield properly.
     * We don't allocate a stack - kmain already has one.
     */
    task_t *bootstrap = kmalloc(sizeof(task_t));
    ASSERT(bootstrap != NULL);
    bootstrap->rsp = 0;  /* Will be saved on first yield */
    bootstrap->next = NULL;
    bootstrap->state = TASK_RUNNING;
    bootstrap->stack_base = NULL;  /* Using Limine-provided stack */
    bootstrap->id = 0;
    bootstrap->entry = NULL;
    /* Initialize user mode fields (bootstrap is kernel task) */
    bootstrap->user_rsp = 0;
    bootstrap->kernel_rsp = 0;
    bootstrap->user_rip = 0;
    bootstrap->is_user = 0;
    bootstrap->user_stack_base = NULL;

    current_task = bootstrap;

    /* Create idle task */
    idle_task = task_create(idle_entry);
    ASSERT(idle_task != NULL);

    /* Link bootstrap and idle in circular queue */
    bootstrap->next = idle_task;
    idle_task->next = bootstrap;

    serial_puts("SCHED: Scheduler initialized\n");
}

void scheduler_add(task_t *task) {
    ASSERT(task != NULL);
    ASSERT(current_task != NULL);

    /* Disable interrupts during queue manipulation */
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));

    /* Insert task after current task in circular queue */
    task->next = current_task->next;
    current_task->next = task;

    /* Restore interrupt state */
    asm volatile("push %0; popfq" : : "r"(flags));
}

void scheduler_yield(void) {
    ASSERT(current_task != NULL);

    /* Save flags and disable interrupts */
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));

    /* Find next READY task (round-robin) */
    task_t *old = current_task;
    task_t *next = old->next;

    /* Skip FINISHED tasks, but always allow idle_task */
    while (next->state == TASK_FINISHED && next != idle_task) {
        next = next->next;
        /* Safety: don't loop forever if all tasks are finished */
        if (next == old->next) {
            next = idle_task;
            break;
        }
    }

    /* If only finished tasks remain (except idle), switch to idle */
    if (next->state == TASK_FINISHED) {
        next = idle_task;
    }

    /* Update states */
    if (old->state == TASK_RUNNING) {
        old->state = TASK_READY;
    }
    next->state = TASK_RUNNING;
    current_task = next;

    /* Perform context switch if switching to different task */
    if (old != next) {
        /* Set TSS RSP0 for user task interrupt handling */
        if (next->is_user && next->kernel_rsp) {
            tss_set_rsp0(next->kernel_rsp);
        }
        context_switch(old, next);
    }

    /* Restore flags (re-enables interrupts if they were enabled) */
    asm volatile("push %0; popfq" : : "r"(flags));
}
