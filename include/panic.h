#ifndef PANIC_H
#define PANIC_H

#include "serial.h"

void panic(const char *msg);

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            serial_puts("ASSERT FAILED: " #cond "\n"); \
            serial_puts("  at " __FILE__ ":"); \
            _assert_print_line(__LINE__); \
            serial_puts("\n"); \
            panic("assertion failed"); \
        } \
    } while (0)

static inline void _assert_print_line(int line) {
    char buf[12];
    int i = 0;
    if (line == 0) {
        serial_putc('0');
        return;
    }
    while (line > 0) {
        buf[i++] = '0' + (line % 10);
        line /= 10;
    }
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

#endif
