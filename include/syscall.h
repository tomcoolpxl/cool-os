#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Syscall numbers */
#define SYS_exit    0
#define SYS_write   1
#define SYS_yield   2
#define SYS_wait    3
#define SYS_getpid  4
#define SYS_getppid 5

/* Initialize SYSCALL/SYSRET mechanism */
void syscall_init(void);

/*
 * Syscall dispatcher - called from syscall_entry.S
 * Arguments follow System V AMD64 ABI:
 *   num in RAX, arg1 in RDI, arg2 in RSI, arg3 in RDX
 * Returns result in RAX
 */
uint64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif
