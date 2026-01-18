#include "elf.h"
#include "paging.h"
#include "pmm.h"
#include "hhdm.h"
#include "serial.h"
#include "panic.h"
#include <stddef.h>

/* User address space limits */
#define USER_ADDR_MIN   0x10000ULL          /* Minimum user address (leave null page unmapped) */
#define USER_ADDR_MAX   0x7FFFFFFFFFFFULL   /* Maximum user address (end of low canonical) */

static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

/*
 * Validate ELF header.
 * Returns 0 if valid, -1 if invalid.
 */
static int elf_validate_header(const Elf64_Ehdr *ehdr, uint64_t file_size) {
    /* Check magic number */
    if (ehdr->e_ident[EI_MAG0] != 0x7f ||
        ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' ||
        ehdr->e_ident[EI_MAG3] != 'F') {
        serial_puts("ELF: Invalid magic number\n");
        return -1;
    }

    /* Check class (must be 64-bit) */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        serial_puts("ELF: Not 64-bit\n");
        return -1;
    }

    /* Check endianness (must be little-endian) */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        serial_puts("ELF: Not little-endian\n");
        return -1;
    }

    /* Check type (must be executable) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        serial_puts("ELF: Not executable\n");
        return -1;
    }

    /* Check machine (must be x86-64) */
    if (ehdr->e_machine != EM_X86_64) {
        serial_puts("ELF: Not x86-64\n");
        return -1;
    }

    /* Check program header offset */
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        serial_puts("ELF: No program headers\n");
        return -1;
    }

    /* Check program header bounds */
    uint64_t ph_end = ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize;
    if (ph_end > file_size) {
        serial_puts("ELF: Program headers extend past file end\n");
        return -1;
    }

    return 0;
}

/*
 * Validate a PT_LOAD segment.
 * Returns 0 if valid, -1 if invalid.
 */
static int elf_validate_segment(const Elf64_Phdr *phdr) {
    /* Check that memsz >= filesz */
    if (phdr->p_memsz < phdr->p_filesz) {
        serial_puts("ELF: memsz < filesz\n");
        return -1;
    }

    /* Check that segment is in user address range */
    if (phdr->p_vaddr < USER_ADDR_MIN) {
        serial_puts("ELF: Segment below user address range: ");
        print_hex(phdr->p_vaddr);
        serial_puts("\n");
        return -1;
    }

    uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
    if (seg_end > USER_ADDR_MAX || seg_end < phdr->p_vaddr) {
        serial_puts("ELF: Segment extends past user address range\n");
        return -1;
    }

    return 0;
}

int elf_load(const void *data, uint64_t size, elf_info_t *info) {
    if (data == NULL || size < sizeof(Elf64_Ehdr) || info == NULL) {
        serial_puts("ELF: Invalid parameters\n");
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

    /* Validate header */
    if (elf_validate_header(ehdr, size) != 0) {
        return -1;
    }

    serial_puts("ELF: Loading executable, entry ");
    print_hex(ehdr->e_entry);
    serial_puts("\n");

    /* Initialize load bounds */
    info->entry = ehdr->e_entry;
    info->load_base = ~0ULL;
    info->load_end = 0;

    const uint8_t *file_data = (const uint8_t *)data;

    /* First pass: validate all segments and calculate bounds */
    int has_load = 0;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_memsz == 0) {
            continue;
        }

        has_load = 1;

        if (elf_validate_segment(phdr) != 0) {
            return -1;
        }

        /* Check file bounds */
        if (phdr->p_offset + phdr->p_filesz > size) {
            serial_puts("ELF: Segment file data extends past file end\n");
            return -1;
        }

        /* Update bounds */
        if (phdr->p_vaddr < info->load_base) {
            info->load_base = phdr->p_vaddr;
        }
        uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
        if (seg_end > info->load_end) {
            info->load_end = seg_end;
        }
    }

    if (!has_load) {
        serial_puts("ELF: No PT_LOAD segments\n");
        return -1;
    }

    /* Validate entry point is within loaded range */
    if (info->entry < info->load_base || info->entry >= info->load_end) {
        serial_puts("ELF: Entry point outside loaded segments\n");
        return -1;
    }

    /* Second pass: map and load segments */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        /* Determine page permissions from p_flags */
        int writable = (phdr->p_flags & PF_W) ? 1 : 0;
        int executable = (phdr->p_flags & PF_X) ? 1 : 0;

        serial_puts("ELF: Loading segment at ");
        print_hex(phdr->p_vaddr);
        serial_puts(" size ");
        print_hex(phdr->p_memsz);
        serial_puts(" flags ");
        if (phdr->p_flags & PF_R) serial_putc('R');
        if (phdr->p_flags & PF_W) serial_putc('W');
        if (phdr->p_flags & PF_X) serial_putc('X');
        serial_puts("\n");

        /* Align start address down to page boundary */
        uint64_t page_start = phdr->p_vaddr & ~0xFFFULL;
        uint64_t page_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFFULL;

        /* Map pages */
        for (uint64_t vaddr = page_start; vaddr < page_end; vaddr += 0x1000) {
            /* Allocate physical frame */
            uint64_t paddr = pmm_alloc_frame();
            if (paddr == 0) {
                serial_puts("ELF: Out of physical memory\n");
                return -1;
            }

            /* Zero the frame first */
            uint8_t *frame_ptr = (uint8_t *)phys_to_hhdm(paddr);
            for (int j = 0; j < 4096; j++) {
                frame_ptr[j] = 0;
            }

            /* Map with user permissions */
            if (paging_map_user_page(vaddr, paddr, writable, executable) != 0) {
                serial_puts("ELF: Failed to map page\n");
                return -1;
            }

            /* Copy file data if this page contains any */
            uint64_t file_start = phdr->p_vaddr;
            uint64_t file_end = phdr->p_vaddr + phdr->p_filesz;

            /* Calculate overlap between this page and file data */
            uint64_t copy_start = vaddr;
            if (copy_start < file_start) {
                copy_start = file_start;
            }
            uint64_t copy_end = vaddr + 0x1000;
            if (copy_end > file_end) {
                copy_end = file_end;
            }

            if (copy_start < copy_end) {
                /* There's data to copy */
                uint64_t page_offset = copy_start - vaddr;
                uint64_t file_offset = phdr->p_offset + (copy_start - phdr->p_vaddr);
                uint64_t copy_len = copy_end - copy_start;

                const uint8_t *src = file_data + file_offset;
                uint8_t *dst = frame_ptr + page_offset;

                for (uint64_t k = 0; k < copy_len; k++) {
                    dst[k] = src[k];
                }
            }
        }
    }

    serial_puts("ELF: Loaded successfully, range ");
    print_hex(info->load_base);
    serial_puts(" - ");
    print_hex(info->load_end);
    serial_puts("\n");

    return 0;
}

int elf_load_at(const void *data, uint64_t size, uint64_t load_addr, elf_info_t *info) {
    if (data == NULL || size < sizeof(Elf64_Ehdr) || info == NULL) {
        serial_puts("ELF: Invalid parameters\n");
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

    /* Validate header */
    if (elf_validate_header(ehdr, size) != 0) {
        return -1;
    }

    const uint8_t *file_data = (const uint8_t *)data;

    /* First pass: find original base address and validate segments */
    uint64_t orig_base = ~0ULL;
    int has_load = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        has_load = 1;

        if (phdr->p_offset + phdr->p_filesz > size) {
            serial_puts("ELF: Segment file data extends past file end\n");
            return -1;
        }

        if (phdr->p_vaddr < orig_base) {
            orig_base = phdr->p_vaddr;
        }
    }

    if (!has_load) {
        serial_puts("ELF: No PT_LOAD segments\n");
        return -1;
    }

    /* Calculate offset to apply to all addresses */
    int64_t addr_offset = (int64_t)load_addr - (int64_t)orig_base;

    serial_puts("ELF: Loading executable at ");
    print_hex(load_addr);
    serial_puts(", entry ");
    print_hex(ehdr->e_entry + addr_offset);
    serial_puts("\n");

    /* Initialize load bounds with offset applied */
    info->entry = ehdr->e_entry + addr_offset;
    info->load_base = ~0ULL;
    info->load_end = 0;

    /* Second pass: map and load segments with offset */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(file_data + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        /* Apply offset to virtual address */
        uint64_t seg_vaddr = phdr->p_vaddr + addr_offset;
        uint64_t seg_end = seg_vaddr + phdr->p_memsz;

        /* Validate adjusted addresses are in user range */
        if (seg_vaddr < USER_ADDR_MIN || seg_end > USER_ADDR_MAX) {
            serial_puts("ELF: Adjusted segment outside user range\n");
            return -1;
        }

        /* Update bounds */
        if (seg_vaddr < info->load_base) {
            info->load_base = seg_vaddr;
        }
        if (seg_end > info->load_end) {
            info->load_end = seg_end;
        }

        /* Determine page permissions from p_flags */
        int writable = (phdr->p_flags & PF_W) ? 1 : 0;
        int executable = (phdr->p_flags & PF_X) ? 1 : 0;

        /* Align start address down to page boundary */
        uint64_t page_start = seg_vaddr & ~0xFFFULL;
        uint64_t page_end = (seg_end + 0xFFF) & ~0xFFFULL;

        /* Map pages */
        for (uint64_t vaddr = page_start; vaddr < page_end; vaddr += 0x1000) {
            /* Allocate physical frame */
            uint64_t paddr = pmm_alloc_frame();
            if (paddr == 0) {
                serial_puts("ELF: Out of physical memory\n");
                return -1;
            }

            /* Zero the frame first */
            uint8_t *frame_ptr = (uint8_t *)phys_to_hhdm(paddr);
            for (int j = 0; j < 4096; j++) {
                frame_ptr[j] = 0;
            }

            /* Map with user permissions */
            if (paging_map_user_page(vaddr, paddr, writable, executable) != 0) {
                serial_puts("ELF: Failed to map page\n");
                return -1;
            }

            /* Copy file data if this page contains any */
            uint64_t file_start = seg_vaddr;
            uint64_t file_end_addr = seg_vaddr + phdr->p_filesz;

            /* Calculate overlap between this page and file data */
            uint64_t copy_start = vaddr;
            if (copy_start < file_start) {
                copy_start = file_start;
            }
            uint64_t copy_end = vaddr + 0x1000;
            if (copy_end > file_end_addr) {
                copy_end = file_end_addr;
            }

            if (copy_start < copy_end) {
                /* There's data to copy */
                uint64_t page_offset = copy_start - vaddr;
                /* File offset = original file offset + (adjusted_addr - original_vaddr) */
                uint64_t file_offset = phdr->p_offset + (copy_start - seg_vaddr);
                uint64_t copy_len = copy_end - copy_start;

                const uint8_t *src = file_data + file_offset;
                uint8_t *dst = frame_ptr + page_offset;

                for (uint64_t k = 0; k < copy_len; k++) {
                    dst[k] = src[k];
                }
            }
        }
    }

    serial_puts("ELF: Loaded successfully, range ");
    print_hex(info->load_base);
    serial_puts(" - ");
    print_hex(info->load_end);
    serial_puts("\n");

    return 0;
}
