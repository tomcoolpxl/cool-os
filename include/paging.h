#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* Page table entry flags */
#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITE_THRU  (1ULL << 3)
#define PTE_CACHE_DIS   (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7)
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)

/* Physical address mask (bits 12-51 for 4-level paging) */
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

/* User-space virtual address base (low canonical) */
#define USER_VADDR_BASE 0x400000ULL

/*
 * Initialize paging subsystem.
 * Saves the kernel's CR3 for later use when creating new address spaces.
 */
void paging_init(void);

/*
 * Get the kernel's master PML4 physical address.
 */
uint64_t paging_get_kernel_cr3(void);

/*
 * Map a physical page at a virtual address with specific flags.
 * Uses current CR3.
 * Returns 0 on success, -1 on failure.
 */
int paging_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);

/*
 * Map a physical page at a virtual address with specific flags into a specific PML4.
 * pml4: virtual address of the PML4 (via HHDM)
 * Returns 0 on success, -1 on failure.
 */
int paging_map_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);

/*
 * Map a physical page at a user-space virtual address.
 * Creates page table entries as needed with U/S=1, W=writable.
 * If executable=0, sets NX bit to prevent code execution.
 * Uses current CR3.
 * Returns 0 on success, -1 on failure.
 */
int paging_map_user_page(uint64_t vaddr, uint64_t paddr, int writable, int executable);

/*
 * Map a physical page at a user-space virtual address into a specific PML4.
 * pml4: virtual address of the PML4 (via HHDM)
 * Returns 0 on success, -1 on failure.
 */
int paging_map_user_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, int writable, int executable);

/*
 * Clone kernel higher-half mappings (PML4 entries 256-511) from kernel PML4 to dst.
 * This includes kernel code/data, HHDM, and framebuffer.
 */
void paging_clone_kernel_mappings(uint64_t *dst_pml4);

/*
 * Free all user-space pages in an address space.
 * Walks PML4 entries 0-255 (user half), frees leaf pages and intermediate tables.
 * Does NOT touch kernel half (entries 256-511).
 */
void paging_free_user_pages(uint64_t *pml4);

/*
 * Flush TLB for a specific virtual address.
 */
void paging_flush_tlb(uint64_t vaddr);

#endif
