#include "pit.h"
#include "ports.h"
#include "serial.h"

#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43

/*
 * Command byte for channel 0, mode 3 (square wave), lo/hi byte access
 * Bits 7-6: 00 = Channel 0
 * Bits 5-4: 11 = Access mode: low byte then high byte
 * Bits 3-1: 011 = Mode 3 (square wave generator)
 * Bit 0:    0 = Binary counting (not BCD)
 */
#define PIT_CMD_CHANNEL0_MODE3  0x36

static volatile uint64_t ticks = 0;

void pit_init(uint32_t hz) {
    /* Calculate divisor: base_freq / desired_freq */
    uint32_t divisor = PIT_FREQ / hz;

    /* Clamp to 16-bit range */
    if (divisor > 65535) {
        divisor = 65535;
    }
    if (divisor < 1) {
        divisor = 1;
    }

    /* Send command byte */
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0_MODE3);

    /* Send divisor (low byte then high byte) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    serial_puts("PIT: Initialized at 100 Hz\n");
}

uint64_t pit_get_ticks(void) {
    return ticks;
}

void pit_tick(void) {
    ticks++;
}
