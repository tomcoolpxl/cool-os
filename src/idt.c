#include "idt.h"
#include "isr.h"
#include "serial.h"

/* IDT with 256 entries (only first 32 used for exceptions) */
static struct idt_entry idt[IDT_ENTRIES];
static struct idtr idtr;

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr) {
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;  /* No IST for now */
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void idt_init(void) {
    /* Install exception handlers for vectors 0-31 */
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_table[i], IDT_TYPE_INTERRUPT_GATE);
    }

    /* Install default handler for vectors 32-255 */
    for (int i = 32; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_default, IDT_TYPE_INTERRUPT_GATE);
    }

    /* Load IDT register */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    asm volatile("lidt %0" : : "m"(idtr));

    serial_puts("IDT: Installed (256 vectors)\n");
}
