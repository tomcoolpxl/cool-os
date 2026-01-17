#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define IS_PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

void pmm_init(void);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t phys_addr);
uint64_t pmm_get_free_frames(void);

#endif
