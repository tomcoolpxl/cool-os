#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"
#include "task.h"
#include "heap.h"
#include "panic.h"
#include "serial.h"
#include "gdt.h"
#include "paging.h"
#include "cpu.h"
#include "isr.h"

/* External: current_task is defined in task.c */
extern task_t *current_task;

/* Assembly context switch function */
extern void context_switch(task_t *old, task_t *new);

/* Idle task: runs when no other tasks are ready */
static task_t *idle_task = NULL;

/*
 * Preemption support (Proto 17).
 *
 * preempt_new_rsp: Reserved for future IRQ-based context switching.
 * Currently unused - preemption uses scheduler_yield() instead.
 * Kept for compatibility with isr_stubs.S which references it.
 */
volatile uint64_t preempt_new_rsp = 0;

/* Reentrancy guard - prevents nested scheduler calls from timer IRQs */
static volatile int in_scheduler = 0;

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
    bootstrap->state = PROC_RUNNING;
    bootstrap->stack_base = NULL;  /* Using Limine-provided stack */
    bootstrap->id = 0;
    bootstrap->entry = NULL;
    /* Initialize user mode fields (bootstrap is kernel task) */
    bootstrap->user_rsp = 0;
    bootstrap->kernel_rsp = 0;
    bootstrap->user_rip = 0;
    bootstrap->is_user = 0;
    bootstrap->user_stack_base = NULL;
    /* Initialize process lifecycle fields (bootstrap is PID 0) */
    bootstrap->pid = 0;
    bootstrap->ppid = 0;
    bootstrap->parent = NULL;
    bootstrap->exit_code = 0;
    bootstrap->first_child = NULL;
    bootstrap->next_sibling = NULL;

    /* Initialize address space fields (bootstrap uses kernel address space) */
    bootstrap->cr3 = paging_get_kernel_cr3();
    bootstrap->pml4 = NULL;  /* Not tracked for kernel tasks */

    /* Initialize preemptive scheduling time slice (Proto 17) */
    bootstrap->ticks_remaining = SCHED_TICK_SLICE;

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

    /* Disable interrupts during scheduling */
    asm volatile("cli");

    /* Find next READY task (round-robin) */
    task_t *old = current_task;
    task_t *next = old->next;

    /* Skip ZOMBIE and BLOCKED tasks, but always allow idle_task */
    while ((next->state == PROC_ZOMBIE || next->state == PROC_BLOCKED) && next != idle_task) {
        next = next->next;
        /* Safety: don't loop forever if no runnable tasks */
        if (next == old->next) {
            next = idle_task;
            break;
        }
    }

    /* If only zombie/blocked tasks remain (except idle), switch to idle */
    if (next->state == PROC_ZOMBIE || next->state == PROC_BLOCKED) {
        next = idle_task;
    }

    /* Update states */
    if (old->state == PROC_RUNNING) {
        old->state = PROC_READY;
    }
    next->state = PROC_RUNNING;
    current_task = next;

    /* Reset time slice for next task (Proto 17) */
    next->ticks_remaining = SCHED_TICK_SLICE;

    /* Perform context switch if switching to different task */
    if (old != next) {
        /* Switch address space if different */
        uint64_t current_cr3 = read_cr3() & PTE_ADDR_MASK;
        if (next->cr3 != 0 && next->cr3 != current_cr3) {
            write_cr3(next->cr3);
        }

        /* Set TSS RSP0 for user task interrupt handling */
        if (next->is_user && next->kernel_rsp) {
            tss_set_rsp0(next->kernel_rsp);
        }
        context_switch(old, next);
    }

    /*
     * Re-enable interrupts (Proto 17).
     *
     * We always enable interrupts here because:
     * 1. Normal yield: task voluntarily yielded, wants interrupts back
     * 2. Preemption: task was preempted from IRQ handler, needs interrupts
     *    re-enabled so future timer interrupts can fire
     *
     * This is safe because:
     * - For user tasks: iretq will restore user RFLAGS anyway
     * - For kernel tasks: they expect to run with IF=1
     */
    asm volatile("sti");
}

/*
 * Preemptive scheduler entry point (Proto 17).
 *
 * Called from timer IRQ handler when current task's time slice expires.
 * This function uses the standard scheduler_yield() path to switch tasks,
 * avoiding the complexity of IRQ-frame vs context-switch frame mismatch.
 *
 * When called for a user task in IRQ context:
 * - The IRQ handler has already saved all registers on the kernel stack
 * - We call scheduler_yield() which will:
 *   - Save current context (via context_switch)
 *   - Pick next task
 *   - Restore next task's context
 * - When we return here, we're in the same task, IRQ handler returns normally
 *
 * The key insight: scheduler_yield() does cooperative switching. When we
 * preempt a user task, we're in kernel mode (in IRQ handler), so we can
 * safely call scheduler_yield(). The user-mode state is already saved
 * on the task's kernel stack by the IRQ entry, and will be restored by iretq.
 *
 * This approach works because:
 * 1. IRQ saves user state on kernel stack
 * 2. scheduler_yield saves kernel state via context_switch
 * 3. Later, context_switch restores kernel state
 * 4. IRQ return restores user state via iretq
 */
void scheduler_preempt(struct interrupt_frame *frame) {
    (void)frame;  /* Not used - context saved by scheduler_yield */

    /* Reentrancy guard - timer could fire while we're in the scheduler */
    if (in_scheduler) {
        return;
    }

    ASSERT(current_task != NULL);

    /*
     * Call the standard yield function. This will:
     * - Save current kernel context
     * - Switch to next ready task
     * - Restore that task's kernel context
     *
     * When we return (if we return), the IRQ handler will continue
     * and iretq will restore user state.
     */
    scheduler_yield();
}
