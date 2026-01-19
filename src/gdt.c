#include "gdt.h"
#include "serial.h"

/*
 * GDT access byte bits:
 *   Bit 7: Present
 *   Bit 6-5: DPL (Descriptor Privilege Level)
 *   Bit 4: Descriptor type (1 = code/data, 0 = system)
 *   Bit 3: Executable (1 = code, 0 = data)
 *   Bit 2: Direction/Conforming
 *   Bit 1: Read/Write
 *   Bit 0: Accessed
 *
 * For 64-bit code: 0x9A (kernel) or 0xFA (user)
 * For 64-bit data: 0x92 (kernel) or 0xF2 (user)
 */

#define GDT_ACCESS_PRESENT     (1 << 7)
#define GDT_ACCESS_DPL_USER    (3 << 5)
#define GDT_ACCESS_CODE_DATA   (1 << 4)
#define GDT_ACCESS_EXECUTABLE  (1 << 3)
#define GDT_ACCESS_READWRITE   (1 << 1)

/* Flags byte (high 4 bits): G=1 (4K granularity), L=1 (64-bit code) */
#define GDT_FLAG_LONG_MODE     (1 << 5)
#define GDT_FLAG_GRANULARITY   (1 << 7)

/* TSS access byte: Present, DPL=0, Type=0x9 (64-bit TSS available) */
#define TSS_ACCESS_PRESENT     0x89

/*
 * GDT with 6 entries (TSS takes 2 slots in 64-bit mode):
 *   0: null
 *   1: kernel code (0x08)
 *   2: kernel data (0x10)
 *   3: user data   (0x18)
 *   4: user code   (0x20)
 *   5-6: TSS       (0x28) - 16 bytes
 *
 * Total: 5 * 8 + 16 = 56 bytes
 */
struct gdt_combined {
    struct gdt_entry entries[5];
    struct gdt_entry_tss tss;
} __attribute__((packed, aligned(8)));

static struct gdt_combined gdt;
static struct gdtr gdtr;
static struct tss tss;

static void gdt_set_entry(int index, uint8_t access, uint8_t flags) {
    /* Base and limit are ignored in 64-bit mode for code/data segments */
    gdt.entries[index].limit_low = 0xFFFF;
    gdt.entries[index].base_low = 0;
    gdt.entries[index].base_mid = 0;
    gdt.entries[index].access = access;
    gdt.entries[index].flags_limit_high = flags | 0x0F;  /* Limit bits 16-19 = 0xF */
    gdt.entries[index].base_high = 0;
}

static void gdt_set_tss(uint64_t base, uint32_t limit) {
    gdt.tss.limit_low = limit & 0xFFFF;
    gdt.tss.base_low = base & 0xFFFF;
    gdt.tss.base_mid = (base >> 16) & 0xFF;
    gdt.tss.access = TSS_ACCESS_PRESENT;
    gdt.tss.flags_limit_high = ((limit >> 16) & 0x0F);
    gdt.tss.base_high = (base >> 24) & 0xFF;
    gdt.tss.base_upper = base >> 32;
    gdt.tss.reserved = 0;
}

void gdt_init(void) {
    /* Entry 0: Null descriptor */
    gdt.entries[0].limit_low = 0;
    gdt.entries[0].base_low = 0;
    gdt.entries[0].base_mid = 0;
    gdt.entries[0].access = 0;
    gdt.entries[0].flags_limit_high = 0;
    gdt.entries[0].base_high = 0;

    /* Entry 1: Kernel Code (0x08) - DPL=0, executable, readable, 64-bit */
    gdt_set_entry(1,
        GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READWRITE,
        GDT_FLAG_LONG_MODE | GDT_FLAG_GRANULARITY);

    /* Entry 2: Kernel Data (0x10) - DPL=0, writable */
    gdt_set_entry(2,
        GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA | GDT_ACCESS_READWRITE,
        GDT_FLAG_GRANULARITY);

    /* Entry 3: User Data (0x18) - DPL=3, writable */
    gdt_set_entry(3,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_USER | GDT_ACCESS_CODE_DATA | GDT_ACCESS_READWRITE,
        GDT_FLAG_GRANULARITY);

    /* Entry 4: User Code (0x20) - DPL=3, executable, readable, 64-bit */
    gdt_set_entry(4,
        GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_USER | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READWRITE,
        GDT_FLAG_LONG_MODE | GDT_FLAG_GRANULARITY);

    /* Initialize TSS */
    tss.reserved0 = 0;
    tss.rsp0 = 0;
    tss.rsp1 = 0;
    tss.rsp2 = 0;
    tss.reserved1 = 0;
    tss.ist1 = 0;
    tss.ist2 = 0;
    tss.ist3 = 0;
    tss.ist4 = 0;
    tss.ist5 = 0;
    tss.ist6 = 0;
    tss.ist7 = 0;
    tss.reserved2 = 0;
    tss.reserved3 = 0;
    tss.iopb_offset = sizeof(struct tss);  /* No I/O bitmap */

    /* Entry 5-6: TSS (0x28) - 16 bytes */
    gdt_set_tss((uint64_t)&tss, sizeof(struct tss) - 1);

    /* Build GDTR */
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    /* Load GDT */
    asm volatile("lgdt %0" : : "m"(gdtr));

    /*
     * Reload segment registers:
     * - CS must be loaded via far return
     * - Data segments loaded directly
     */
    asm volatile(
        /* Push new CS and return address for far return */
        "pushq %0\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        /* Load data segment registers */
        "movw %1, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        :
        : "i"((uint64_t)KERNEL_CS), "i"((uint16_t)KERNEL_DS)
        : "rax", "memory"
    );

    /* Load TSS */
    asm volatile("ltr %0" : : "r"((uint16_t)TSS_SEL));
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
