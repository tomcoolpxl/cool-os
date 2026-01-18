#include <stddef.h>
#include "paging.h"
#include "cpu.h"
#include "hhdm.h"
#include "pmm.h"
#include "serial.h"

/*
 * Page table structure for x86-64 4-level paging:
 *   PML4 (level 4) -> PDPT (level 3) -> PD (level 2) -> PT (level 1) -> Page
 *
 * Virtual address bits:
 *   47:39 - PML4 index (9 bits)
 *   38:30 - PDPT index (9 bits)
 *   29:21 - PD index   (9 bits)
 *   20:12 - PT index   (9 bits)
 *   11:0  - Page offset (12 bits)
 */

#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

/*
 * Allocate a zeroed page for page table use.
 * Returns virtual address via HHDM, or NULL on failure.
 */
static uint64_t *alloc_page_table(void) {
    uint64_t phys = pmm_alloc_frame();
    if (phys == 0) {
        return NULL;
    }
    uint64_t *virt = (uint64_t *)phys_to_hhdm(phys);
    /* Zero the page */
    for (int i = 0; i < 512; i++) {
        virt[i] = 0;
    }
    return virt;
}

int paging_map_user_page(uint64_t vaddr, uint64_t paddr, int writable, int executable) {
    uint64_t cr3 = read_cr3() & PTE_ADDR_MASK;
    uint64_t *pml4 = (uint64_t *)phys_to_hhdm(cr3);

    int pml4_idx = PML4_INDEX(vaddr);
    int pdpt_idx = PDPT_INDEX(vaddr);
    int pd_idx   = PD_INDEX(vaddr);
    int pt_idx   = PT_INDEX(vaddr);

    /* Level 4: PML4 -> PDPT */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t *new_pdpt = alloc_page_table();
        if (!new_pdpt) return -1;
        uint64_t new_pdpt_phys = hhdm_to_phys(new_pdpt);
        pml4[pml4_idx] = new_pdpt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        /* Ensure U/S bit is set on existing entry */
        pml4[pml4_idx] |= PTE_USER;
    }

    uint64_t *pdpt = (uint64_t *)phys_to_hhdm(pml4[pml4_idx] & PTE_ADDR_MASK);

    /* Level 3: PDPT -> PD */
    if (pdpt[pdpt_idx] & PTE_HUGE) {
        /* 1GB huge page - can't map 4K page here */
        serial_puts("paging: cannot map over 1GB huge page\n");
        return -1;
    }
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t *new_pd = alloc_page_table();
        if (!new_pd) return -1;
        uint64_t new_pd_phys = hhdm_to_phys(new_pd);
        pdpt[pdpt_idx] = new_pd_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        pdpt[pdpt_idx] |= PTE_USER;
    }

    uint64_t *pd = (uint64_t *)phys_to_hhdm(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    /* Level 2: PD -> PT */
    if (pd[pd_idx] & PTE_HUGE) {
        /* 2MB huge page - can't map 4K page here */
        serial_puts("paging: cannot map over 2MB huge page\n");
        return -1;
    }
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t *new_pt = alloc_page_table();
        if (!new_pt) return -1;
        uint64_t new_pt_phys = hhdm_to_phys(new_pt);
        pd[pd_idx] = new_pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        pd[pd_idx] |= PTE_USER;
    }

    uint64_t *pt = (uint64_t *)phys_to_hhdm(pd[pd_idx] & PTE_ADDR_MASK);

    /* Level 1: PT -> Page */
    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (writable) {
        flags |= PTE_WRITABLE;
    }
    if (!executable) {
        flags |= PTE_NX;
    }
    pt[pt_idx] = (paddr & PTE_ADDR_MASK) | flags;

    /* Flush TLB for this address */
    paging_flush_tlb(vaddr);

    return 0;
}

void paging_flush_tlb(uint64_t vaddr) {
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}
