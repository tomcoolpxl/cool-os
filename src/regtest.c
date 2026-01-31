#include "regtest.h"
#include "serial.h"
#include "ports.h"
#include <stdarg.h>

/*
 * Regression Test Infrastructure Implementation
 *
 * Provides logging, pass/fail tracking, and QEMU exit via isa-debug-exit.
 */

/* Global test counters */
static int total_passed = 0;
static int total_failed = 0;
static int suite_passed = 0;
static int suite_failed = 0;

/*
 * Exit QEMU with test result via isa-debug-exit device.
 * QEMU exit code = (value << 1) | 1
 * - success=1: write 0x00 -> exit code 1
 * - success=0: write 0x01 -> exit code 3
 */
void regtest_exit(int success) {
    uint8_t code = success ? REGTEST_SUCCESS : REGTEST_FAILURE;
    regtest_log("EXIT %d\n", success ? 1 : 3);
    /* Use outb to write to isa-debug-exit device.
     * QEMU exit code = (code << 1) | 1
     * code=0x00 -> QEMU exits with 1 (success)
     * code=0x01 -> QEMU exits with 3 (failure)
     */
    outb(REGTEST_EXIT_PORT, code);
    /* Should not reach here - QEMU should have exited */
    for (;;) {
        asm volatile("hlt");
    }
}

/*
 * Print a decimal number to serial.
 */
static void print_dec(int val) {
    if (val < 0) {
        serial_putc('-');
        val = -val;
    }
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

/*
 * Print a hex number to serial.
 */
static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xf;
        if (digit || started || i == 0) {
            serial_putc(hex[digit]);
            started = 1;
        }
    }
}

/*
 * Simple printf-like logging with [REGTEST] prefix.
 * Supports: %s, %d, %x, %p
 */
void regtest_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    serial_puts("[REGTEST] ");

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(ap, const char *);
                    serial_puts(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    int d = va_arg(ap, int);
                    print_dec(d);
                    break;
                }
                case 'x': {
                    uint64_t x = va_arg(ap, uint64_t);
                    print_hex(x);
                    break;
                }
                case 'p': {
                    void *p = va_arg(ap, void *);
                    print_hex((uint64_t)p);
                    break;
                }
                case '%':
                    serial_putc('%');
                    break;
                default:
                    serial_putc('%');
                    serial_putc(*fmt);
                    break;
            }
        } else {
            serial_putc(*fmt);
        }
        fmt++;
    }

    va_end(ap);
}

/*
 * Record a passing test.
 */
void regtest_pass(const char *test_name) {
    regtest_log("PASS %s\n", test_name);
    suite_passed++;
    total_passed++;
}

/*
 * Record a failing test with reason.
 */
void regtest_fail(const char *test_name, const char *reason) {
    regtest_log("FAIL %s: %s\n", test_name, reason);
    suite_failed++;
    total_failed++;
}

/*
 * Mark the start of a test suite.
 */
void regtest_start_suite(const char *suite_name) {
    suite_passed = 0;
    suite_failed = 0;
    regtest_log("START %s\n", suite_name);
}

/*
 * Mark the end of a test suite.
 */
void regtest_end_suite(const char *suite_name) {
    regtest_log("END %s passed=%d failed=%d\n", suite_name, suite_passed, suite_failed);
}

/*
 * Run all enabled test suites and return overall result.
 * Only available when REGTEST_BUILD is defined.
 */
#ifdef REGTEST_BUILD
int regtest_run_all(void) {
    int result = 0;

    regtest_log("=== cool-os Regression Test Suite ===\n");

#ifdef REGTEST_PMM
    if (regtest_pmm() != 0) result = -1;
#endif

#ifdef REGTEST_HEAP
    if (regtest_heap() != 0) result = -1;
#endif

#ifdef REGTEST_TASK
    if (regtest_task() != 0) result = -1;
#endif

#ifdef REGTEST_USER
    if (regtest_user() != 0) result = -1;
#endif

#ifdef REGTEST_ELF
    if (regtest_elf() != 0) result = -1;
#endif

#ifdef REGTEST_FS
    if (regtest_fs() != 0) result = -1;
#endif

#ifdef REGTEST_FB
    if (regtest_fb() != 0) result = -1;
#endif

#ifdef REGTEST_CONSOLE
    if (regtest_console() != 0) result = -1;
#endif

#ifdef REGTEST_KBD
    if (regtest_kbd() != 0) result = -1;
#endif

#ifdef REGTEST_SHELL
    if (regtest_shell() != 0) result = -1;
#endif

    regtest_log("SUMMARY total=%d passed=%d failed=%d\n",
                total_passed + total_failed, total_passed, total_failed);

    return result;
}
#endif /* REGTEST_BUILD */
