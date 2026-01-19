#include <stdint.h>
#include "timer.h"
#include "serial.h"
#include "framebuffer.h"
#include "console.h"
#include "kbd.h"
#include "utils.h"
#include "pmm.h"
#include "heap.h"
#include "panic.h"
#include "hhdm.h"

// Forward declarations for test functions
void test_pmm(void);
void test_heap(void);
void test_exception_ud(void);
void test_exception_pf(void);
void test_graphics_and_console(void);
void test_keyboard(void);

/*
 * Main test runner called by kmain in test builds.
 */
void run_kernel_tests(void) {
    serial_puts("\n=== Running All Kernel Tests ===\n");

    test_pmm();
    test_heap();

    // Note: Exception tests will halt execution, so they should be run
    // selectively if other tests are needed. For now, we call them all.
    // In a more advanced test runner, we might use command-line args
    // from the bootloader to select which test to run.

    // test_exception_ud();
    // test_exception_pf();

    test_graphics_and_console();
    test_keyboard();

    serial_puts("\n=== All Kernel Tests Complete ===\n");
}

/*
 * Tests the Physical Memory Manager (PMM).
 */
void test_pmm(void) {
    serial_puts("\n--- PMM Validation Test ---\n");
    uint64_t test_frames[10];
    uint64_t free_before = pmm_get_free_frames();
    for (int i = 0; i < 10; i++) {
        test_frames[i] = pmm_alloc_frame();
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
    serial_puts("PMM: All 10 frames allocated and verified successfully.\n");
}

/*
 * Tests the Kernel Heap allocator.
 */
void test_heap(void) {
    serial_puts("\n--- Heap Validation Test ---\n");
    /* Test 1: Basic alloc/free */
    void *p1 = kmalloc(64);
    ASSERT(p1 != NULL);
    void *p2 = kmalloc(128);
    ASSERT(p2 != NULL);
    void *p3 = kmalloc(256);
    ASSERT(p3 != NULL);
    kfree(p2);
    kfree(p1);
    kfree(p3);
    serial_puts("HEAP: Basic allocation test passed.\n");

    /* Test 2: Coalescing test */
    void *c1 = kmalloc(100);
    void *c2 = kmalloc(100);
    void *c3 = kmalloc(100);
    ASSERT(c1 != NULL && c2 != NULL && c3 != NULL);
    kfree(c2);
    kfree(c1);
    kfree(c3);
    void *big = kmalloc(300);
    ASSERT(big != NULL);
    kfree(big);
    serial_puts("HEAP: Coalescing test passed.\n");

    /* Test 3: Stress test */
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(32);
        ASSERT(ptrs[i] != NULL);
    }
    for (int i = 0; i < 100; i += 2) {
        kfree(ptrs[i]);
    }
    for (int i = 0; i < 100; i += 2) {
        ptrs[i] = kmalloc(32);
        ASSERT(ptrs[i] != NULL);
    }
    for (int i = 0; i < 100; i++) {
        kfree(ptrs[i]);
    }
    serial_puts("HEAP: Stress test passed.\n");
}

/*
 * Triggers an invalid opcode exception.
 */
void test_exception_ud(void) {
    serial_puts("TEST: Triggering #UD (invalid opcode)...");
    asm volatile("ud2");
}

/*
 * Triggers a page fault exception.
 */
void test_exception_pf(void) {
    serial_puts("TEST: Triggering #PF (page fault)...");
    *(volatile uint64_t *)0xdeadbeefdeadbeef = 1;
}

/*
 * Contains all tests for framebuffer graphics (Proto 10) and the
 * text console (Proto 11).
 */
void test_graphics_and_console(void) {
    serial_puts("\n=== PROTO10 TESTS (Framebuffer) ===\n");

    const framebuffer_t *fb_info = fb_get_info();
    if (fb_info == NULL) {
        serial_puts("PROTO10: Framebuffer not initialized, skipping tests\n");
    } else {
        /* Test 1: Solid fill (blue screen) */
        serial_puts("PROTO10 TEST1: Solid fill (blue screen)\n");
        fb_clear(0x000066CC);  /* Blue */
        fb_present();
        timer_sleep_ms(1000);
        serial_puts("PROTO10 TEST1: Complete\n");

        /* Test 2: Moving rectangle animation */
        serial_puts("PROTO10 TEST2: Moving rectangle animation\n");
        fb_clear(0x00002244);

        uint32_t rect_x = 0;
        uint32_t rect_y = fb_info->render_height / 2 - 50;
        uint32_t max_x = fb_info->render_width - 100;
        int direction = 1;
        uint64_t start_ticks = timer_get_ticks();
        uint64_t end_ticks = start_ticks + (3 * TIMER_HZ);
        uint32_t anim_frames = 0;

        while (timer_get_ticks() < end_ticks) {
            fb_clear(0x00002244);
            fb_fill_rect(rect_x, rect_y, 100, 100, 0x00FFFFFF);
            fb_present();
            anim_frames++;
            rect_x += direction * 8;
            if (rect_x >= max_x) {
                direction = -1;
                rect_x = max_x;
            } else if (direction == -1 && rect_x <= 8) {
                direction = 1;
                rect_x = 0;
            }
        }
        serial_puts("PROTO10 TEST2: Complete\n");

        /* Test 3: Color cycling */
        serial_puts("PROTO10 TEST4: Color cycle test\n");
        uint32_t colors[] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFF00};
        for (int i = 0; i < 4; i++) {
            fb_clear(colors[i]);
            fb_present();
            timer_sleep_ms(500);
        }
        serial_puts("PROTO10 TEST4: Complete\n");
    }

    serial_puts("\n=== PROTO11 TESTS (Text Console) ===\n");

    if (fb_get_info() == NULL) {
        serial_puts("PROTO11: Framebuffer not initialized, skipping tests\n");
    } else {
        /* Test 1: Basic text output */
        serial_puts("PROTO11 TEST1: Basic text output\n");
        console_clear();
        console_puts("Hello from console!\n");
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST1: Complete\n");

        /* Test 2: Scrolling test */
        serial_puts("PROTO11 TEST3: Scrolling test\n");
        console_clear();
        for (int i = 0; i < 60; i++) {
            console_puts("Scroll test line\n");
        }
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST3: Complete\n");

        /* Test 3: Clear screen test */
        serial_puts("PROTO11 TEST4: Clear screen test\n");
        console_clear();
        console_puts("Screen cleared! This is the only text.\n");
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST4: Complete\n");
    }
}

/*
 * Contains all tests for PS/2 keyboard input (Proto 12).
 */
void test_keyboard(void) {
    serial_puts("\n=== PROTO12 TESTS (Keyboard Input) ===\n");

    if (fb_get_info() == NULL) {
        serial_puts("PROTO12: Framebuffer not initialized, skipping tests\n");
    } else {
        /* Test 1: Raw input echo */
        serial_puts("PROTO12 TEST1: Raw input echo\n");
        console_clear();
        console_puts("Type keys, ESC to stop:\n");
        while (1) {
            char c = kbd_getc_blocking();
            if (c == 27) break;  /* ESC */
            console_putc(c);
            fb_present();
        }
        console_puts("\n");
        serial_puts("PROTO12 TEST1: Complete\n");

        /* Test 2: Line input */
        serial_puts("PROTO12 TEST2: Line input\n");
        char name[64];
        console_puts("Enter your name: ");
        kbd_readline(name, 64);
        console_puts("\nHello, ");
        console_puts(name);
        console_puts("!\n");
        serial_puts("PROTO12 TEST2: Complete\n");
    }
}
