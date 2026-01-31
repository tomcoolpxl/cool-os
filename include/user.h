#ifndef USER_H
#define USER_H

#include <stdint.h>
#include "syscall.h"

/*
 * User-mode syscall wrappers
 *
 * These inline functions use the SYSCALL instruction to invoke kernel services.
 * SYSCALL ABI:
 *   RAX = syscall number
 *   RDI = arg1
 *   RSI = arg2
 *   RDX = arg3
 *   Returns: RAX
 *   Clobbers: RCX (return RIP), R11 (return RFLAGS)
 */

static inline void user_exit(uint64_t code) {
    asm volatile(
        "syscall"
        :
        : "a"(SYS_exit), "D"(code)
        : "rcx", "r11", "memory"
    );
    /* Should not return, but prevent compiler warnings */
    __builtin_unreachable();
}

static inline uint64_t user_write(uint64_t fd, const char *buf, uint64_t len) {
    uint64_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(SYS_write), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void user_yield(void) {
    asm volatile(
        "syscall"
        :
        : "a"(SYS_yield)
        : "rcx", "r11", "memory"
    );
}

static inline int user_wait(int *status) {
    int ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(SYS_wait), "D"(status)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint32_t user_getpid(void) {
    uint32_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(SYS_getpid)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint32_t user_getppid(void) {
    uint32_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(SYS_getppid)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif
