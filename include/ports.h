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

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Read count 16-bit words from port into buffer.
 * Used for ATA PIO data transfers.
 */
static inline void insw(uint16_t port, void *buf, uint32_t count) {
    asm volatile("rep insw" : "+D"(buf), "+c"(count) : "d"(port) : "memory");
}

/*
 * I/O delay using port 0x80 (POST diagnostic port).
 * Writing to this port takes ~1 microsecond on most hardware.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
