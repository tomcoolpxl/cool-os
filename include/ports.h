#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

/*
 * x86 I/O port access routines
 */

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * I/O delay using port 0x80 (POST diagnostic port).
 * Writing to this port takes ~1 microsecond on most hardware.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
