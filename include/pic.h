#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/*
 * 8259A Programmable Interrupt Controller (PIC) driver
 *
 * The legacy PIC consists of two cascaded chips:
 * - Master PIC: IRQ0-7 -> remapped to vectors 0x20-0x27
 * - Slave PIC:  IRQ8-15 -> remapped to vectors 0x28-0x2F
 */

#define PIC1_COMMAND  0x20
#define PIC1_DATA     0x21
#define PIC2_COMMAND  0xA0
#define PIC2_DATA     0xA1

#define PIC_EOI       0x20  /* End-of-interrupt command */

#define IRQ_VECTOR_BASE  0x20  /* Master PIC base vector */

/* Initialize and remap the PICs */
void pic_init(void);

/* Send end-of-interrupt signal to appropriate PIC(s) */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) an IRQ line */
void pic_set_mask(uint8_t irq);

/* Clear mask (enable) an IRQ line */
void pic_clear_mask(uint8_t irq);

#endif
