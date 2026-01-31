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

/* Kernel's master PML4 (physical address) */
static uint64_t kernel_cr3 = 0;

void paging_init(void) {
    kernel_cr3 = read_cr3() & PTE_ADDR_MASK;
    serial_puts("PAGING: Saved kernel CR3: ");
    /* Simple hex print */
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(kernel_cr3 >> i) & 0xf]);
    }
    serial_puts("\n");
}

uint64_t paging_get_kernel_cr3(void) {
    return kernel_cr3;
}

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

int paging_map_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    int pml4_idx = PML4_INDEX(vaddr);
    int pdpt_idx = PDPT_INDEX(vaddr);
    int pd_idx   = PD_INDEX(vaddr);
    int pt_idx   = PT_INDEX(vaddr);

    /* Determine intermediate flags based on leaf flags */
    /* Always keep intermediate tables Present and Writable */
    uint64_t inter_flags = PTE_PRESENT | PTE_WRITABLE;
    if (flags & PTE_USER) {
        inter_flags |= PTE_USER;
    }

    /* Level 4: PML4 -> PDPT */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t *new_pdpt = alloc_page_table();
        if (!new_pdpt) return -1;
        uint64_t new_pdpt_phys = hhdm_to_phys(new_pdpt);
        pml4[pml4_idx] = new_pdpt_phys | inter_flags;
    } else {
        /* Add User bit if requested */
        if (flags & PTE_USER) pml4[pml4_idx] |= PTE_USER;
    }

    uint64_t *pdpt = (uint64_t *)phys_to_hhdm(pml4[pml4_idx] & PTE_ADDR_MASK);

    /* Level 3: PDPT -> PD */
    if (pdpt[pdpt_idx] & PTE_HUGE) {
        serial_puts("paging: cannot map over 1GB huge page\n");
        return -1;
    }
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t *new_pd = alloc_page_table();
        if (!new_pd) return -1;
        uint64_t new_pd_phys = hhdm_to_phys(new_pd);
        pdpt[pdpt_idx] = new_pd_phys | inter_flags;
    } else {
        if (flags & PTE_USER) pdpt[pdpt_idx] |= PTE_USER;
    }

    uint64_t *pd = (uint64_t *)phys_to_hhdm(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    /* Level 2: PD -> PT */
    if (pd[pd_idx] & PTE_HUGE) {
        serial_puts("paging: cannot map over 2MB huge page\n");
        return -1;
    }
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t *new_pt = alloc_page_table();
        if (!new_pt) return -1;
        uint64_t new_pt_phys = hhdm_to_phys(new_pt);
        pd[pd_idx] = new_pt_phys | inter_flags;
    } else {
        if (flags & PTE_USER) pd[pd_idx] |= PTE_USER;
    }

    uint64_t *pt = (uint64_t *)phys_to_hhdm(pd[pd_idx] & PTE_ADDR_MASK);

    /* Level 1: PT -> Page */
    pt[pt_idx] = (paddr & PTE_ADDR_MASK) | flags;

    /* Flush TLB only if this is the current address space */
    uint64_t current_cr3 = read_cr3() & PTE_ADDR_MASK;
    if (hhdm_to_phys(pml4) == current_cr3) {
        paging_flush_tlb(vaddr);
    }

    return 0;
}

int paging_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t cr3 = read_cr3() & PTE_ADDR_MASK;
    uint64_t *pml4 = (uint64_t *)phys_to_hhdm(cr3);
    return paging_map_page_in(pml4, vaddr, paddr, flags);
}

int paging_map_user_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, int writable, int executable) {
    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (writable) flags |= PTE_WRITABLE;
    if (!executable) flags |= PTE_NX;
    return paging_map_page_in(pml4, vaddr, paddr, flags);
}

int paging_map_user_page(uint64_t vaddr, uint64_t paddr, int writable, int executable) {
    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (writable) flags |= PTE_WRITABLE;
    if (!executable) flags |= PTE_NX;
    return paging_map_page(vaddr, paddr, flags);
}

void paging_clone_kernel_mappings(uint64_t *dst_pml4) {
    /* Get kernel PML4 via HHDM */
    uint64_t *src_pml4 = (uint64_t *)phys_to_hhdm(kernel_cr3);

    /* Copy entries 256-511 (higher half) from kernel to destination */
    for (int i = 256; i < 512; i++) {
        dst_pml4[i] = src_pml4[i];
    }
}

void paging_free_user_pages(uint64_t *pml4) {
    /* Walk PML4 entries 0-255 (user half) and free all pages */
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(pml4[pml4_idx] & PTE_PRESENT)) {
            continue;
        }

        uint64_t *pdpt = (uint64_t *)phys_to_hhdm(pml4[pml4_idx] & PTE_ADDR_MASK);

        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
                continue;
            }

            /* Handle 1GB huge pages */
            if (pdpt[pdpt_idx] & PTE_HUGE) {
                /* 1GB page - we don't allocate these for user space, skip */
                continue;
            }

            uint64_t *pd = (uint64_t *)phys_to_hhdm(pdpt[pdpt_idx] & PTE_ADDR_MASK);

            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd[pd_idx] & PTE_PRESENT)) {
                    continue;
                }

                /* Handle 2MB huge pages */
                if (pd[pd_idx] & PTE_HUGE) {
                    /* 2MB page - we don't allocate these for user space, skip */
                    continue;
                }

                uint64_t *pt = (uint64_t *)phys_to_hhdm(pd[pd_idx] & PTE_ADDR_MASK);

                /* Free all leaf pages in this page table */
                for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
                    if (pt[pt_idx] & PTE_PRESENT) {
                        uint64_t page_phys = pt[pt_idx] & PTE_ADDR_MASK;
                        pmm_free_frame(page_phys);
                    }
                }

                /* Free the page table itself */
                uint64_t pt_phys = pd[pd_idx] & PTE_ADDR_MASK;
                pmm_free_frame(pt_phys);
            }

            /* Free the page directory */
            uint64_t pd_phys = pdpt[pdpt_idx] & PTE_ADDR_MASK;
            pmm_free_frame(pd_phys);
        }

        /* Free the PDPT */
        uint64_t pdpt_phys = pml4[pml4_idx] & PTE_ADDR_MASK;
        pmm_free_frame(pdpt_phys);
    }
}

void paging_flush_tlb(uint64_t vaddr) {
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}
