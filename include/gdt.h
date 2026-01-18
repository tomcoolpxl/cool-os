#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*
 * Segment selectors
 * Layout: Null(0x00), KernelCode(0x08), KernelData(0x10),
 *         UserData(0x18), UserCode(0x20), TSS(0x28)
 *
 * User Data before User Code satisfies SYSRET requirements:
 *   SYSRET loads SS = STAR[63:48] + 8, CS = STAR[63:48] + 16
 */
#define KERNEL_CS   0x08
#define KERNEL_DS   0x10
#define USER_DS     0x1B    /* 0x18 | 3 (RPL=3) */
#define USER_CS     0x23    /* 0x20 | 3 (RPL=3) */
#define TSS_SEL     0x28

/* 8-byte GDT entry for code/data segments */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} __attribute__((packed));

/* 16-byte GDT entry for TSS (system segment in long mode) */
struct gdt_entry_tss {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

/* GDTR register format */
struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/*
 * Task State Segment (TSS) for x86-64
 * Primarily used for RSP0 (kernel stack for ring transitions)
 */
struct tss {
    uint32_t reserved0;
    uint64_t rsp0;          /* Stack pointer for CPL 0 */
    uint64_t rsp1;          /* Stack pointer for CPL 1 (unused) */
    uint64_t rsp2;          /* Stack pointer for CPL 2 (unused) */
    uint64_t reserved1;
    uint64_t ist1;          /* Interrupt Stack Table entries */
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;   /* I/O permission bitmap offset */
} __attribute__((packed));

/* Initialize GDT with kernel/user segments and TSS */
void gdt_init(void);

/* Set TSS RSP0 for interrupt stack switching */
void tss_set_rsp0(uint64_t rsp0);

#endif
