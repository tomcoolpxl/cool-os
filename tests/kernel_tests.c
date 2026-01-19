#include "serial.h"
#include "timer.h"
#include "task.h"
#include "pmm.h"
#include "heap.h"
#include "assert.h"
#include "console.h"
#include "framebuffer.h"
#include "syscall.h"
#include "block.h"
#include "fat32.h"
#include "vfs.h"
#include "utils.h"
#include "hhdm.h"
#include "panic.h"
#include <stddef.h>

/* Proto 6 test tasks */
static void test_task_a(void) {
    for (int i = 0; i < 5; i++) {
        serial_puts("A\n");
        timer_sleep_ms(500);
        task_yield();
    }
}

static void test_task_b(void) {
    for (int i = 0; i < 5; i++) {
        serial_puts("B\n");
        timer_sleep_ms(500);
        task_yield();
    }
}

static void test_task_exit(void) {
    serial_puts("done\n");
}

void run_pmm_tests(void) {
    serial_puts("PMM: Running allocation test...\n");
    uint64_t test_frames[10];
    uint64_t free_before = pmm_get_free_frames();
    for (int i = 0; i < 10; i++) {
        test_frames[i] = pmm_alloc_frame();
        serial_puts("PMM: Allocated frame ");
        print_hex((uint64_t)i);
        serial_puts(" at ");
        print_hex(test_frames[i]);
        serial_puts("\n");

        /* Write and verify test pattern */
        volatile uint64_t *v = (volatile uint64_t *)phys_to_hhdm(test_frames[i]);
        *v = 0xCAFEBABECAFEBABEULL;
        ASSERT(*v == 0xCAFEBABECAFEBABEULL);
    }
    /* Free all test frames */
    for (int i = 0; i < 10; i++) {
        pmm_free_frame(test_frames[i]);
    }
    uint64_t free_after = pmm_get_free_frames();
    ASSERT(free_before == free_after);
    serial_puts("PMM: All 10 frames allocated and verified successfully\n");
    serial_puts("PMM: Free frames restored: ");
    print_hex(free_after);
    serial_puts("\n");
}

void run_heap_tests(void) {
    serial_puts("HEAP: Running basic allocation test...\n");
    void *p1 = kmalloc(64);
    ASSERT(p1 != NULL);
    serial_puts("HEAP: Allocated 64 bytes at ");
    print_hex((uint64_t)p1);
    serial_puts("\n");

    void *p2 = kmalloc(128);
    ASSERT(p2 != NULL);
    serial_puts("HEAP: Allocated 128 bytes at ");
    print_hex((uint64_t)p2);
    serial_puts("\n");

    kfree(p1);
    kfree(p2);
    serial_puts("HEAP: Basic allocation test passed\n");
}