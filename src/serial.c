#include "serial.h"
#include "ports.h"

#define COM1_PORT 0x3F8

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

/* Helper to reverse a string */
static void str_reverse(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void serial_print_dec(uint64_t val) {
    char buf[21];
    int i = 0;

    if (val == 0) {
        serial_putc('0');
        return;
    }

    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    
    buf[i] = '\0';
    str_reverse(buf, i);
    serial_puts(buf);
}

void serial_print_hex(uint64_t val) {
    char buf[17];
    const char *hex_chars = "0123456789abcdef";
    int i = 0;

    serial_puts("0x");

    if (val == 0) {
        serial_putc('0');
        return;
    }

    while (val > 0) {
        buf[i++] = hex_chars[val % 16];
        val /= 16;
    }

    buf[i] = '\0';
    str_reverse(buf, i);
    serial_puts(buf);
}

