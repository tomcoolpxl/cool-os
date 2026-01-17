#include <stdint.h>
#include <stddef.h>
#include "pmm.h"
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "serial.h"
#include "panic.h"
#include "hhdm.h"

/* Global Limine response pointers (set by kernel.c) */
extern struct limine_memmap_response *limine_memmap;
extern struct limine_executable_address_response *limine_exec_addr;

/* PMM state */
static uint8_t *pmm_bitmap;      /* HHDM virtual address of bitmap */
static uint64_t pmm_frame_count; /* Total frames in system */
static uint64_t pmm_bitmap_size; /* Bitmap size in bytes */
static uint64_t pmm_free_frames; /* Current free frame count */

/* Freestanding memset */
static void pmm_memset(void *dest, uint8_t val, uint64_t count) {
    uint8_t *d = (uint8_t *)dest;
    for (uint64_t i = 0; i < count; i++) {
        d[i] = val;
    }
}

/* Bitmap helpers */
static inline void bitmap_set(uint64_t frame) {
    pmm_bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint64_t frame) {
    pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline int bitmap_test(uint64_t frame) {
    return (pmm_bitmap[frame / 8] >> (frame % 8)) & 1;
}

/* Output helpers */
static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static void print_dec(uint64_t val) {
    char buf[21];
    int i = 0;
    if (val == 0) {
        serial_putc('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

/* Get memory map type name for debugging */
static const char *memmap_type_name(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE: return "USABLE";
        case LIMINE_MEMMAP_RESERVED: return "RESERVED";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return "ACPI_RECLAIMABLE";
        case LIMINE_MEMMAP_ACPI_NVS: return "ACPI_NVS";
        case LIMINE_MEMMAP_BAD_MEMORY: return "BAD_MEMORY";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "BOOTLOADER_RECLAIMABLE";
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return "EXECUTABLE_AND_MODULES";
        case LIMINE_MEMMAP_FRAMEBUFFER: return "FRAMEBUFFER";
        default: return "UNKNOWN";
    }
}

void pmm_init(void) {
    serial_puts("PMM: Initializing physical memory manager\n");

    ASSERT(limine_memmap != NULL);
    ASSERT(limine_exec_addr != NULL);

    /* Step 1: Find max physical address from USABLE regions only.
     * This avoids creating a huge bitmap for sparse MMIO regions. */
    uint64_t max_phys_addr = 0;
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        /* Only consider allocatable memory for max address */
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
            uint64_t end = entry->base + entry->length;
            if (end > max_phys_addr) {
                max_phys_addr = end;
            }
        }
    }

    /* Step 2: Compute frame count and bitmap size */
    pmm_frame_count = max_phys_addr / PAGE_SIZE;
    pmm_bitmap_size = (pmm_frame_count + 7) / 8;

    /* Step 3: Find first USABLE region large enough for bitmap */
    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= pmm_bitmap_size) {
            bitmap_phys = entry->base;
            break;
        }
    }
    ASSERT(bitmap_phys != 0);

    /* Convert bitmap physical address to HHDM virtual address */
    pmm_bitmap = (uint8_t *)phys_to_hhdm(bitmap_phys);
    ASSERT(pmm_bitmap != NULL);

    /* Step 4: Mark all frames as used (1 = used, 0 = free) */
    pmm_memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    pmm_free_frames = 0;

    /* Step 5: Free USABLE regions (clear bits) */
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_frame = entry->base / PAGE_SIZE;
            uint64_t end_frame = (entry->base + entry->length) / PAGE_SIZE;
            for (uint64_t frame = start_frame; frame < end_frame; frame++) {
                bitmap_clear(frame);
                pmm_free_frames++;
            }
        }
    }

    /* Step 6: Re-mark bitmap frames as used */
    uint64_t bitmap_start_frame = bitmap_phys / PAGE_SIZE;
    uint64_t bitmap_end_frame = (bitmap_phys + pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t frame = bitmap_start_frame; frame < bitmap_end_frame; frame++) {
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            pmm_free_frames--;
        }
    }

    /* Step 7: Reserve kernel frames using exec_addr */
    uint64_t kernel_phys_base = limine_exec_addr->physical_base;
    uint64_t kernel_size = 0;

    /* Find kernel size from EXECUTABLE_AND_MODULES memmap entry */
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES &&
            entry->base == kernel_phys_base) {
            kernel_size = entry->length;
            break;
        }
    }

    if (kernel_size > 0) {
        uint64_t kernel_start_frame = kernel_phys_base / PAGE_SIZE;
        uint64_t kernel_end_frame = (kernel_phys_base + kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t frame = kernel_start_frame; frame < kernel_end_frame; frame++) {
            if (frame < pmm_frame_count && !bitmap_test(frame)) {
                bitmap_set(frame);
                pmm_free_frames--;
            }
        }
    }

    /* Step 8: Print summary */
    serial_puts("PMM: Physical memory manager initialized\n");
    serial_puts("PMM: Max physical address: ");
    print_hex(max_phys_addr);
    serial_puts("\n");
    serial_puts("PMM: Total frames: ");
    print_dec(pmm_frame_count);
    serial_puts("\n");
    serial_puts("PMM: Free frames: ");
    print_dec(pmm_free_frames);
    serial_puts("\n");
    serial_puts("PMM: Bitmap at phys ");
    print_hex(bitmap_phys);
    serial_puts(", virt ");
    print_hex((uint64_t)pmm_bitmap);
    serial_puts("\n");

    /* Print memory map for debugging */
    serial_puts("PMM: Memory map entries:\n");
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        serial_puts("  ");
        print_hex(entry->base);
        serial_puts(" - ");
        print_hex(entry->base + entry->length);
        serial_puts(" ");
        serial_puts(memmap_type_name(entry->type));
        serial_puts("\n");
    }
}

uint64_t pmm_alloc_frame(void) {
    /* Linear search for first free frame */
    for (uint64_t frame = 0; frame < pmm_frame_count; frame++) {
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            pmm_free_frames--;
            uint64_t phys_addr = frame * PAGE_SIZE;
            ASSERT(IS_PAGE_ALIGNED(phys_addr));
            return phys_addr;
        }
    }
    panic("PMM: Out of memory!");
    return 0; /* Unreachable */
}

void pmm_free_frame(uint64_t phys_addr) {
    ASSERT(IS_PAGE_ALIGNED(phys_addr));
    uint64_t frame = phys_addr / PAGE_SIZE;
    ASSERT(frame < pmm_frame_count);
    ASSERT(bitmap_test(frame)); /* Double-free detection */
    bitmap_clear(frame);
    pmm_free_frames++;
}

uint64_t pmm_get_free_frames(void) {
    return pmm_free_frames;
}
