#ifndef HHDM_H
#define HHDM_H

#include <stdint.h>

extern uint64_t hhdm_offset;

void hhdm_init(uint64_t offset);

static inline void *phys_to_hhdm(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static inline uint64_t hhdm_to_phys(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}

#endif
