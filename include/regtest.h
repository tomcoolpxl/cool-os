#ifndef REGTEST_H
#define REGTEST_H

#include <stdint.h>

/*
 * Regression Test Infrastructure for cool-os
 *
 * Provides CI-friendly automated testing with QEMU exit codes:
 * - Port 0x501 (isa-debug-exit): Write N -> QEMU exits with (N << 1) | 1
 * - Write 0x00 -> exit code 1 (tests passed)
 * - Write 0x01 -> exit code 3 (tests failed)
 *
 * Output format (parsed by scripts/run_regtest.sh):
 *   [REGTEST] START suite_name
 *   [REGTEST] PASS test_name
 *   [REGTEST] FAIL test_name: reason
 *   [REGTEST] END suite_name passed=N failed=M
 *   [REGTEST] SUMMARY total=N passed=P failed=F
 *   [REGTEST] EXIT code
 */

/* QEMU isa-debug-exit port */
#define REGTEST_EXIT_PORT 0x501

/* Exit codes (QEMU exit = (code << 1) | 1) */
#define REGTEST_SUCCESS 0x00  /* QEMU exits with 1 */
#define REGTEST_FAILURE 0x01  /* QEMU exits with 3 */

/*
 * Test flags for selective suite execution.
 * Define these via -D compiler flags (e.g., -DREGTEST_PMM).
 * REGTEST_ALL runs all suites.
 */
#ifdef REGTEST_ALL
#define REGTEST_PMM     1
#define REGTEST_HEAP    1
#define REGTEST_TASK    1
#define REGTEST_USER    1
#define REGTEST_ELF     1
#define REGTEST_FS      1
#define REGTEST_FB      1
#define REGTEST_CONSOLE 1
#define REGTEST_KBD     1
#define REGTEST_SHELL   1
#define REGTEST_LIBC    1
#define REGTEST_PROCESS 1
#endif

/* Default: run all tests if no specific suite is selected */
#if !defined(REGTEST_PMM) && !defined(REGTEST_HEAP) && !defined(REGTEST_TASK) && \
    !defined(REGTEST_USER) && !defined(REGTEST_ELF) && !defined(REGTEST_FS) && \
    !defined(REGTEST_FB) && !defined(REGTEST_CONSOLE) && !defined(REGTEST_KBD) && \
    !defined(REGTEST_SHELL) && !defined(REGTEST_LIBC) && !defined(REGTEST_PROCESS)
#define REGTEST_PMM     1
#define REGTEST_HEAP    1
#define REGTEST_TASK    1
#define REGTEST_USER    1
#define REGTEST_ELF     1
#define REGTEST_FS      1
#define REGTEST_FB      1
#define REGTEST_CONSOLE 1
#define REGTEST_KBD     1
#define REGTEST_SHELL   1
#define REGTEST_LIBC    1
#define REGTEST_PROCESS 1
#endif

/*
 * Exit QEMU with test result.
 * success: 1 = all tests passed, 0 = one or more failed
 */
void regtest_exit(int success);

/*
 * Log a message with [REGTEST] prefix to serial output.
 * Supports basic format specifiers: %s, %d, %x, %p
 */
void regtest_log(const char *fmt, ...);

/*
 * Record a passing test.
 */
void regtest_pass(const char *test_name);

/*
 * Record a failing test with reason.
 */
void regtest_fail(const char *test_name, const char *reason);

/*
 * Mark the start of a test suite.
 */
void regtest_start_suite(const char *suite_name);

/*
 * Mark the end of a test suite (prints pass/fail counts).
 */
void regtest_end_suite(const char *suite_name);

/*
 * Run all enabled test suites.
 * Returns 0 on success, -1 on failure.
 */
int regtest_run_all(void);

/*
 * Individual test suite entry points.
 * Each returns 0 on success, -1 on failure.
 */
int regtest_pmm(void);
int regtest_heap(void);
int regtest_task(void);
int regtest_user(void);
int regtest_elf(void);
int regtest_fs(void);
int regtest_fb(void);
int regtest_console(void);
int regtest_kbd(void);
int regtest_shell(void);
int regtest_libc(void);
int regtest_process(void);

#endif /* REGTEST_H */
