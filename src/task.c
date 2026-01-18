#include <stdint.h>
#include <stddef.h>
#include "task.h"
#include "scheduler.h"
#include "pmm.h"
#include "heap.h"
#include "hhdm.h"
#include "panic.h"
#include "gdt.h"
#include "paging.h"
#include "serial.h"
#include "elf.h"

static uint64_t next_task_id = 0;

/* Counter for unique user virtual address allocation per task */
static uint64_t next_user_slot = 0;

/* Current task pointer - exported for scheduler */
task_t *current_task = NULL;

/* Forward declarations */
static void task_trampoline(void);
static void user_task_trampoline(void);

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

    /* Initialize user mode fields (not used for kernel tasks) */
    task->user_rsp = 0;
    task->kernel_rsp = 0;
    task->user_rip = 0;
    task->is_user = 0;
    task->user_stack_base = NULL;

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

/*
 * User task trampoline: transitions from kernel mode to user mode.
 * Called via context_switch when a user task starts.
 */
static void user_task_trampoline(void) {
    /* Enable interrupts */
    asm volatile("sti");

    /* Set TSS RSP0 so interrupts use this task's kernel stack */
    tss_set_rsp0(current_task->kernel_rsp);

    /*
     * Build iretq frame on stack and execute iretq to enter user mode.
     * Stack must contain (from top):
     *   SS     (user data segment)
     *   RSP    (user stack pointer)
     *   RFLAGS (with IF set for interrupts)
     *   CS     (user code segment)
     *   RIP    (user entry point)
     */
    asm volatile(
        "movq %0, %%rax\n\t"        /* User RIP */
        "movq %1, %%rbx\n\t"        /* User RSP */
        "pushq %2\n\t"              /* SS = USER_DS */
        "pushq %%rbx\n\t"           /* RSP = user stack */
        "pushq %3\n\t"              /* RFLAGS with IF set */
        "pushq %4\n\t"              /* CS = USER_CS */
        "pushq %%rax\n\t"           /* RIP = user entry */
        "iretq\n\t"
        :
        : "r"(current_task->user_rip),
          "r"(current_task->user_rsp),
          "i"((uint64_t)USER_DS),
          "i"((uint64_t)0x202),      /* RFLAGS: IF=1, reserved bit 1=1 */
          "i"((uint64_t)USER_CS)
        : "rax", "rbx", "memory"
    );

    /* Should never reach here */
    ASSERT(0);
}

task_t *task_create_user(const void *code, uint64_t code_size) {
    ASSERT(code != NULL);
    ASSERT(code_size > 0 && code_size <= TASK_STACK_SIZE);

    /* Allocate task struct from heap */
    task_t *task = kmalloc(sizeof(task_t));
    ASSERT(task != NULL);

    /* Allocate kernel stack from PMM (for syscalls and interrupts) */
    uint64_t kernel_stack_phys = pmm_alloc_frame();
    ASSERT(kernel_stack_phys != 0);
    void *kernel_stack_base = (void *)phys_to_hhdm(kernel_stack_phys);

    /* Allocate user code page from PMM */
    uint64_t user_code_phys = pmm_alloc_frame();
    ASSERT(user_code_phys != 0);

    /* Allocate user stack page from PMM */
    uint64_t user_stack_phys = pmm_alloc_frame();
    ASSERT(user_stack_phys != 0);

    /* Get unique slot for this task's user virtual addresses */
    uint64_t slot = next_user_slot++;

    /* Calculate user virtual addresses for this task
     * Each task gets its own code page and stack page at unique addresses
     * Code:  0x400000 + slot * 0x10000 (64KB apart)
     * Stack: 0x800000 + slot * 0x10000
     */
    uint64_t user_code_vaddr = USER_CODE_VADDR + slot * 0x10000;
    uint64_t user_stack_vaddr = USER_STACK_VADDR + slot * 0x10000;

    /* Map user code page at user virtual address (read-only, executable) */
    int ret = paging_map_user_page(user_code_vaddr, user_code_phys, 0, 1);
    ASSERT(ret == 0);

    /* Map user stack page at user virtual address (read-write, non-executable) */
    ret = paging_map_user_page(user_stack_vaddr, user_stack_phys, 1, 0);
    ASSERT(ret == 0);

    /* Copy user code to the user code page (via HHDM for kernel access) */
    uint8_t *code_dst = (uint8_t *)phys_to_hhdm(user_code_phys);
    const uint8_t *code_src = (const uint8_t *)code;
    for (uint64_t i = 0; i < code_size; i++) {
        code_dst[i] = code_src[i];
    }

    /* Zero the user stack page */
    uint8_t *stack_page = (uint8_t *)phys_to_hhdm(user_stack_phys);
    for (int i = 0; i < TASK_STACK_SIZE; i++) {
        stack_page[i] = 0;
    }

    task->stack_base = kernel_stack_base;
    task->entry = NULL;  /* Not used for user tasks */
    task->state = TASK_READY;
    task->id = next_task_id++;
    task->next = NULL;

    /* User mode fields - all user-space virtual addresses */
    task->is_user = 1;
    task->user_rip = user_code_vaddr;  /* Start of user code page */
    task->user_stack_base = (void *)user_stack_vaddr;
    /* User stack grows down; start at top of user stack page */
    task->user_rsp = user_stack_vaddr + TASK_STACK_SIZE;
    /* Kernel stack top for syscalls/interrupts */
    task->kernel_rsp = (uint64_t)kernel_stack_base + TASK_STACK_SIZE;

    /*
     * Set up kernel stack frame for context_switch.
     * Stack grows downward, so start at top of allocated region.
     * First context_switch will "return" to user_task_trampoline,
     * which then does iretq to enter user mode.
     */
    uint64_t *sp = (uint64_t *)((uint8_t *)kernel_stack_base + TASK_STACK_SIZE);

    /* Return address - where context_switch's ret will jump */
    *(--sp) = (uint64_t)user_task_trampoline;

    /* Callee-saved registers (pushed in order: rbp, rbx, r12, r13, r14, r15) */
    *(--sp) = 0;  /* r15 */
    *(--sp) = 0;  /* r14 */
    *(--sp) = 0;  /* r13 */
    *(--sp) = 0;  /* r12 */
    *(--sp) = 0;  /* rbx */
    *(--sp) = 0;  /* rbp */

    task->rsp = (uint64_t)sp;

    return task;
}

/* Counter for unique ELF user address allocation per task */
static uint64_t next_elf_slot = 0;

/* ELF code base address and spacing between tasks */
#define ELF_CODE_BASE   0x1000000ULL    /* 16 MB - base for ELF code */
#define ELF_SLOT_SIZE   0x100000ULL     /* 1 MB spacing between ELF programs */

task_t *task_create_elf(const void *data, uint64_t size) {
    if (data == NULL || size == 0) {
        serial_puts("task_create_elf: Invalid parameters\n");
        return NULL;
    }

    /* Get unique slot for this task's code address */
    uint64_t elf_slot = next_elf_slot++;
    uint64_t load_addr = ELF_CODE_BASE + elf_slot * ELF_SLOT_SIZE;

    /* Load the ELF executable into user address space at unique address */
    elf_info_t elf_info;
    if (elf_load_at(data, size, load_addr, &elf_info) != 0) {
        serial_puts("task_create_elf: ELF load failed\n");
        return NULL;
    }

    /* Allocate task struct from heap */
    task_t *task = kmalloc(sizeof(task_t));
    if (task == NULL) {
        serial_puts("task_create_elf: Out of memory for task struct\n");
        return NULL;
    }

    /* Allocate kernel stack from PMM (for syscalls and interrupts) */
    uint64_t kernel_stack_phys = pmm_alloc_frame();
    if (kernel_stack_phys == 0) {
        kfree(task);
        serial_puts("task_create_elf: Out of memory for kernel stack\n");
        return NULL;
    }
    void *kernel_stack_base = (void *)phys_to_hhdm(kernel_stack_phys);

    /* Use same slot for stack (already allocated for code) */
    uint64_t stack_slot = elf_slot;

    /* Calculate user stack base address (each task gets separate stack region) */
    uint64_t user_stack_top = USER_ELF_STACK_TOP - stack_slot * (USER_ELF_STACK_PAGES * 0x1000 + 0x1000);
    uint64_t user_stack_base = user_stack_top - USER_ELF_STACK_PAGES * 0x1000;

    /* Allocate and map user stack pages */
    for (int i = 0; i < USER_ELF_STACK_PAGES; i++) {
        uint64_t stack_phys = pmm_alloc_frame();
        if (stack_phys == 0) {
            /* TODO: cleanup already allocated pages */
            kfree(task);
            serial_puts("task_create_elf: Out of memory for user stack\n");
            return NULL;
        }

        /* Zero the stack page */
        uint8_t *stack_page = (uint8_t *)phys_to_hhdm(stack_phys);
        for (int j = 0; j < 4096; j++) {
            stack_page[j] = 0;
        }

        /* Map user stack page (read-write, non-executable) */
        uint64_t page_vaddr = user_stack_base + i * 0x1000;
        if (paging_map_user_page(page_vaddr, stack_phys, 1, 0) != 0) {
            kfree(task);
            serial_puts("task_create_elf: Failed to map user stack\n");
            return NULL;
        }
    }

    /* Initialize task struct */
    task->stack_base = kernel_stack_base;
    task->entry = NULL;  /* Not used for user tasks */
    task->state = TASK_READY;
    task->id = next_task_id++;
    task->next = NULL;

    /* User mode fields */
    task->is_user = 1;
    task->user_rip = elf_info.entry;
    task->user_stack_base = (void *)user_stack_base;

    /*
     * Set up user stack:
     * - RSP must be 16-byte aligned at user entry
     * - Push a fake return address (0) so if user code tries to return, it faults
     */
    task->user_rsp = user_stack_top - 8;  /* Space for fake return address */

    /* Kernel stack top for syscalls/interrupts */
    task->kernel_rsp = (uint64_t)kernel_stack_base + TASK_STACK_SIZE;

    /*
     * Set up kernel stack frame for context_switch.
     * First context_switch will "return" to user_task_trampoline,
     * which then does iretq to enter user mode.
     */
    uint64_t *sp = (uint64_t *)((uint8_t *)kernel_stack_base + TASK_STACK_SIZE);

    /* Return address - where context_switch's ret will jump */
    *(--sp) = (uint64_t)user_task_trampoline;

    /* Callee-saved registers */
    *(--sp) = 0;  /* r15 */
    *(--sp) = 0;  /* r14 */
    *(--sp) = 0;  /* r13 */
    *(--sp) = 0;  /* r12 */
    *(--sp) = 0;  /* rbx */
    *(--sp) = 0;  /* rbp */

    task->rsp = (uint64_t)sp;

    return task;
}
