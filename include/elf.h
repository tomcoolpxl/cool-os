#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* ELF Magic */
#define ELF_MAGIC       0x464C457F  /* "\x7FELF" as little-endian uint32 */

/* ELF Class (e_ident[EI_CLASS]) */
#define ELFCLASS64      2

/* ELF Data encoding (e_ident[EI_DATA]) */
#define ELFDATA2LSB     1           /* Little-endian */

/* ELF Type (e_type) */
#define ET_EXEC         2           /* Executable */
#define ET_DYN          3           /* Shared object (PIE) */

/* ELF Machine (e_machine) */
#define EM_X86_64       62

/* Program header type (p_type) */
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_PHDR         6

/* Program header flags (p_flags) */
#define PF_X            0x1         /* Executable */
#define PF_W            0x2         /* Writable */
#define PF_R            0x4         /* Readable */

/* e_ident indices */
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_NIDENT       16

/* ELF64 Header */
typedef struct {
    uint8_t     e_ident[EI_NIDENT]; /* ELF identification */
    uint16_t    e_type;             /* Object file type */
    uint16_t    e_machine;          /* Machine type */
    uint32_t    e_version;          /* Object file version */
    uint64_t    e_entry;            /* Entry point address */
    uint64_t    e_phoff;            /* Program header offset */
    uint64_t    e_shoff;            /* Section header offset */
    uint32_t    e_flags;            /* Processor-specific flags */
    uint16_t    e_ehsize;           /* ELF header size */
    uint16_t    e_phentsize;        /* Program header entry size */
    uint16_t    e_phnum;            /* Number of program headers */
    uint16_t    e_shentsize;        /* Section header entry size */
    uint16_t    e_shnum;            /* Number of section headers */
    uint16_t    e_shstrndx;         /* Section name string table index */
} __attribute__((packed)) Elf64_Ehdr;

/* ELF64 Program Header */
typedef struct {
    uint32_t    p_type;             /* Segment type */
    uint32_t    p_flags;            /* Segment flags */
    uint64_t    p_offset;           /* Offset in file */
    uint64_t    p_vaddr;            /* Virtual address */
    uint64_t    p_paddr;            /* Physical address (unused) */
    uint64_t    p_filesz;           /* Size in file */
    uint64_t    p_memsz;            /* Size in memory */
    uint64_t    p_align;            /* Alignment */
} __attribute__((packed)) Elf64_Phdr;

/* ELF load result */
typedef struct {
    uint64_t    entry;              /* Entry point */
    uint64_t    load_base;          /* Lowest mapped address */
    uint64_t    load_end;           /* Highest mapped address + 1 */
} elf_info_t;

/*
 * Validate and load an ELF64 executable into user address space.
 *
 * data: pointer to ELF file in memory
 * size: size of ELF file
 * info: output structure filled on success
 *
 * Returns 0 on success, -1 on failure.
 */
int elf_load(const void *data, uint64_t size, elf_info_t *info);

/*
 * Load an ELF64 executable at a specific base address (for multi-program support).
 * The ELF's virtual addresses are offset by (load_addr - original_base).
 *
 * data: pointer to ELF file in memory
 * size: size of ELF file
 * load_addr: desired base address for loading
 * info: output structure filled on success
 *
 * Returns 0 on success, -1 on failure.
 */
int elf_load_at(const void *data, uint64_t size, uint64_t load_addr, elf_info_t *info);

/*
 * Load an ELF64 executable into a specific address space.
 *
 * data: pointer to ELF file in memory
 * size: size of ELF file
 * pml4: virtual address of the target PML4 (via HHDM)
 * info: output structure filled on success
 *
 * Returns 0 on success, -1 on failure.
 */
int elf_load_into(const void *data, uint64_t size, uint64_t *pml4, elf_info_t *info);

#endif
