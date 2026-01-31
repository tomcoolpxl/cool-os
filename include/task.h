#ifndef TASK_H
#define TASK_H

#include <stdint.h>

/* Process states */
typedef enum {
    PROC_READY,      /* Ready to run */
    PROC_RUNNING,    /* Currently executing */
    PROC_BLOCKED,    /* Waiting for event (e.g., child exit) */
    PROC_ZOMBIE      /* Exited, waiting to be reaped */
} proc_state_t;

/* Legacy state aliases for compatibility */
#define TASK_RUNNING   PROC_RUNNING
#define TASK_READY     PROC_READY
#define TASK_FINISHED  PROC_ZOMBIE

#define TASK_STACK_SIZE  4096  /* 4 KiB per task (1 PMM frame) */

typedef struct task {
    uint64_t rsp;           /* Saved stack pointer - MUST be at offset 0 */
    struct task *next;      /* Next in scheduler queue (circular) */
    proc_state_t state;     /* Process state */
    void *stack_base;       /* Stack allocation base (kernel stack) */
    uint64_t id;            /* Task ID for debugging */
    void (*entry)(void);    /* Entry point function */
    /* User mode fields - offsets must match syscall_entry.S */
    uint64_t user_rsp;      /* offset 48: User stack pointer */
    uint64_t kernel_rsp;    /* offset 56: Kernel stack top for syscalls/interrupts */
    uint64_t user_rip;      /* offset 64: User entry point */
    int is_user;            /* offset 72: 1 if user mode task */
    void *user_stack_base;  /* offset 80: User stack allocation base */

    /* Process lifecycle fields (Proto 15) */
    uint32_t pid;           /* Process ID (unique, non-zero) */
    uint32_t ppid;          /* Parent process ID (0 if no parent) */
    struct task *parent;    /* Parent task pointer (for wakeup on exit) */
    int exit_code;          /* Exit status (valid in PROC_ZOMBIE state) */
    struct task *first_child;   /* Head of children list */
    struct task *next_sibling;  /* Next sibling in parent's child list */

    /* Address space fields (Proto 16) */
    uint64_t cr3;           /* Physical address of PML4 */
    uint64_t *pml4;         /* Virtual address of PML4 (via HHDM) */
} task_t;

task_t *task_create(void (*entry)(void));

/*
 * Create a user-mode task with code at user-space virtual address.
 * code: pointer to machine code bytes (in kernel space)
 * code_size: size of code in bytes
 * The code is copied to a user-accessible page at USER_CODE_VADDR.
 * User stack is mapped at USER_STACK_VADDR.
 */
task_t *task_create_user(const void *code, uint64_t code_size);

/* User-space virtual addresses for task memory layout */
#define USER_CODE_VADDR  0x400000ULL   /* User code starts here */
#define USER_STACK_VADDR 0x800000ULL   /* User stack region (grows down from top) */

/* ELF user stack configuration */
#define USER_ELF_STACK_TOP   0x70000000ULL  /* Top of ELF user stack */
#define USER_ELF_STACK_PAGES 4              /* 16 KiB stack */

/*
 * Create a user-mode task from an ELF64 executable.
 * data: pointer to ELF file data in memory
 * size: size of ELF file
 * Returns task on success, NULL on failure.
 */
task_t *task_create_elf(const void *data, uint64_t size);

/*
 * Create a user-mode task from an ELF file on disk.
 * path: File path (e.g., "INIT.ELF")
 * Returns task on success, NULL on failure.
 */
task_t *task_create_from_path(const char *path);

void task_yield(void);
task_t *task_current(void);

/* Process lifecycle API (Proto 15) */
uint32_t task_getpid(void);
uint32_t task_getppid(void);
int task_wait(int *status);           /* Wait for any child to exit */
void task_set_parent(task_t *child, task_t *parent);
task_t *task_find_by_pid(uint32_t pid);
void task_reap(task_t *zombie);       /* Free zombie task resources */
void task_exit(int code);             /* Exit current task with code */

#endif
