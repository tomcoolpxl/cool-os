#ifndef MSR_H
#define MSR_H

#include <stdint.h>

/* Model-Specific Register addresses */
#define MSR_IA32_EFER       0xC0000080  /* Extended Feature Enable Register */
#define MSR_IA32_STAR       0xC0000081  /* SYSCALL target address */
#define MSR_IA32_LSTAR      0xC0000082  /* Long mode SYSCALL target */
#define MSR_IA32_CSTAR      0xC0000083  /* Compat mode SYSCALL target (unused) */
#define MSR_IA32_FMASK      0xC0000084  /* SYSCALL flag mask */

/* EFER bits */
#define EFER_SCE            (1 << 0)    /* SYSCALL Enable */
#define EFER_LME            (1 << 8)    /* Long Mode Enable */
#define EFER_LMA            (1 << 10)   /* Long Mode Active */

/* Read a Model-Specific Register */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/* Write a Model-Specific Register */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

#endif
