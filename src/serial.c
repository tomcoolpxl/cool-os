#include "serial.h"

#define COM1_PORT 0x3F8

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);  /* Disable all interrupts */
    outb(COM1_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x01);  /* Set divisor to 1 (115200 baud) lo byte */
    outb(COM1_PORT + 1, 0x00);  /*                                 hi byte */
    outb(COM1_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7);  /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putc(char c) {
    while (!serial_transmit_empty())
        ;
    outb(COM1_PORT, c);
}

void serial_puts(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}
