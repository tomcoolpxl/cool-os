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
    void *stack_base;       /* Stack allocation base (kernel stack) */
    uint64_t id;            /* Task ID for debugging */
    void (*entry)(void);    /* Entry point function */
    /* User mode fields - offsets must match syscall_entry.S */
    uint64_t user_rsp;      /* offset 48: User stack pointer */
    uint64_t kernel_rsp;    /* offset 56: Kernel stack top for syscalls/interrupts */
    uint64_t user_rip;      /* offset 64: User entry point */
    int is_user;            /* offset 72: 1 if user mode task */
    void *user_stack_base;  /* offset 80: User stack allocation base */
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
void task_yield(void);
task_t *task_current(void);

#endif
