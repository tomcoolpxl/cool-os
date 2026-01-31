/*
 * syscall.c - C syscall wrappers
 *
 * Provides POSIX-like function interfaces for system calls.
 */

#include <stddef.h>
#include <stdint.h>

/* Syscall numbers (must match kernel's syscall.h) */
#define SYS_exit    0
#define SYS_write   1
#define SYS_yield   2
#define SYS_wait    3
#define SYS_getpid  4
#define SYS_getppid 5

/* Assembly syscall stubs */
extern long _syscall0(long num);
extern long _syscall1(long num, long arg1);
extern long _syscall3(long num, long arg1, long arg2, long arg3);

void exit(int code) {
    _syscall1(SYS_exit, code);
    /* Should not return, but loop forever if it does */
    for (;;) {
        __asm__ volatile("hlt");
    }
}

ssize_t write(int fd, const void *buf, size_t len) {
    return (ssize_t)_syscall3(SYS_write, fd, (long)buf, len);
}

void yield(void) {
    _syscall0(SYS_yield);
}

int wait(int *status) {
    return (int)_syscall1(SYS_wait, (long)status);
}

uint32_t getpid(void) {
    return (uint32_t)_syscall0(SYS_getpid);
}

uint32_t getppid(void) {
    return (uint32_t)_syscall0(SYS_getppid);
}
