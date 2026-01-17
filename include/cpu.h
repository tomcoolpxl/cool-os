#ifndef CPU_H
#define CPU_H

#include <stdint.h>

static inline uint64_t read_cr2(void) {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void cpu_halt(void) {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

#endif
