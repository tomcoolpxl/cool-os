#include "pic.h"
#include "ports.h"
#include "serial.h"

/*
 * ICW1 (Initialization Command Word 1)
 * Bit 4: 1 = Initialization required
 * Bit 0: 1 = ICW4 needed
 */
#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01

/*
 * ICW4 (Initialization Command Word 4)
 * Bit 0: 1 = 8086/88 mode (vs MCS-80/85)
 */
#define ICW4_8086  0x01

void pic_init(void) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    (void)mask1;
    (void)mask2;

    /*
     * Start initialization sequence (ICW1)
     * Both PICs need to be initialized together
     */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /*
     * ICW2: Set vector offset
     * Master PIC: vectors 0x20-0x27
     * Slave PIC:  vectors 0x28-0x2F
     */
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    /*
     * ICW3: Configure cascading
     * Master: Slave is connected to IRQ2 (bit 2 = 1 -> 0x04)
     * Slave:  Cascade identity = 2
     */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /*
     * ICW4: Set mode
     * 8086/88 mode for both
     */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /*
     * Set masks: Mask all IRQs except IRQ0 (timer)
     * Master: 0xFE = 11111110 (only IRQ0 enabled)
     * Slave:  0xFF = 11111111 (all masked)
     */
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);

    serial_puts("PIC: Remapped to 0x20/0x28\n");
}

void pic_send_eoi(uint8_t irq) {
    /* If IRQ came from slave PIC (IRQ8-15), send EOI to both */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    /* Always send EOI to master */
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
