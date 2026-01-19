#include "utils.h"
#include "serial.h"

void print_hex(uint64_t val) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        serial_putc(hex[(val >> (i * 4)) & 0xF]);
    }
    serial_putc('\n');
}