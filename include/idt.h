#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256
#define KERNEL_CS   0x28

/* Type attributes for IDT gates */
#define IDT_TYPE_INTERRUPT_GATE 0x8E  /* P=1, DPL=0, Type=0xE (64-bit interrupt gate) */
#define IDT_TYPE_TRAP_GATE      0x8F  /* P=1, DPL=0, Type=0xF (64-bit trap gate) */

struct idt_entry {
    uint16_t offset_low;    /* bits 0-15 of handler address */
    uint16_t selector;      /* kernel code segment selector */
    uint8_t  ist;           /* interrupt stack table (0 = none) */
    uint8_t  type_attr;     /* type and attributes */
    uint16_t offset_mid;    /* bits 16-31 of handler address */
    uint32_t offset_high;   /* bits 32-63 of handler address */
    uint32_t reserved;      /* must be zero */
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr);

#endif
