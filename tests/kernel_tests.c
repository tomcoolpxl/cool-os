#include <stdint.h>
#include <stddef.h>
#include "timer.h"
#include "serial.h"
#include "framebuffer.h"
#include "console.h"
#include "kbd.h"
#include "pmm.h"
#include "heap.h"
#include "panic.h"
#include "hhdm.h"
#include "task.h"
#include "scheduler.h"
#include "elf.h"
#include "vfs.h"
#include "limine.h"

// External Limine module response
extern struct limine_module_response *limine_modules;
extern struct limine_file *find_module(const char *name);

// Forward declarations for test functions
void test_pmm(void);
void test_heap(void);
void test_exception_ud(void);
void test_exception_pf(void);
void test_task(void);
void test_user(void);
void test_elf(void);
void test_filesystem(void);
void test_graphics_and_console(void);
void test_keyboard(void);

/*
 * Main test runner called by kmain in test builds.
 */
void run_kernel_tests(void) {
    serial_puts("\n=== Running All Kernel Tests ===\n");

    /* Proto 2-3: Memory subsystem tests */
    test_pmm();
    test_heap();

    /* Proto 6-9: Task and user mode tests */
    test_task();
    test_user();
    test_elf();
    test_filesystem();

    /* Proto 10-12: Graphics and input tests (interactive) */
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

/* ========== Proto 6-9 Test Implementations ========== */

/* Proto 6 test task functions */
static void test_task_a_fn(void) {
    for (int i = 0; i < 5; i++) {
        serial_puts("A\n");
        timer_sleep_ms(500);
        task_yield();
    }
}

static void test_task_b_fn(void) {
    for (int i = 0; i < 5; i++) {
        serial_puts("B\n");
        timer_sleep_ms(500);
        task_yield();
    }
}

static void test_task_exit_fn(void) {
    serial_puts("done\n");
}

/*
 * Proto 6: Cooperative Multitasking tests
 */
void test_task(void) {
    serial_puts("\n=== PROTO6 TESTS (Cooperative Multitasking) ===\n");

    /* Test 1: Two task alternation */
    serial_puts("PROTO6 TEST1: Two task alternation\n");
    task_t *task_a = task_create(test_task_a_fn);
    task_t *task_b = task_create(test_task_b_fn);
    scheduler_add(task_a);
    scheduler_add(task_b);

    /* Yield repeatedly to let tasks run */
    while (task_a->state != TASK_FINISHED || task_b->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO6 TEST1: Complete\n");

    /* Test 2: Task exit handling */
    serial_puts("PROTO6 TEST2: Task exit handling\n");
    task_t *task_exit = task_create(test_task_exit_fn);
    scheduler_add(task_exit);
    while (task_exit->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO6 TEST2: Complete\n");

    /* Test 3: Idle fallback */
    serial_puts("PROTO6 TEST3: Idle fallback - entering idle\n");
}

/* Proto 7 user programs as raw machine code */
static const uint8_t user_hello_code[] = {
    0x48, 0x8d, 0x35, 0x17, 0x00, 0x00, 0x00,   /* lea rsi, [rip+23] */
    0xbf, 0x01, 0x00, 0x00, 0x00,               /* mov edi, 1 */
    0xba, 0x16, 0x00, 0x00, 0x00,               /* mov edx, 22 */
    0xb8, 0x01, 0x00, 0x00, 0x00,               /* mov eax, 1 */
    0x0f, 0x05,                                 /* syscall */
    0x31, 0xff,                                 /* xor edi, edi */
    0x31, 0xc0,                                 /* xor eax, eax */
    0x0f, 0x05,                                 /* syscall */
    'H','e','l','l','o',' ','f','r','o','m',' ',
    'u','s','e','r',' ','m','o','d','e','!','\n'
};

static const uint8_t user_yield_code1[] = {
    0x41, 0xbc, 0x03, 0x00, 0x00, 0x00,
    0x48, 0x8d, 0x35, 0x23, 0x00, 0x00, 0x00,
    0xbf, 0x01, 0x00, 0x00, 0x00,
    0xba, 0x03, 0x00, 0x00, 0x00,
    0xb8, 0x01, 0x00, 0x00, 0x00,
    0x0f, 0x05,
    0xb8, 0x02, 0x00, 0x00, 0x00,
    0x0f, 0x05,
    0x41, 0xff, 0xcc,
    0x75, 0xdc,
    0x31, 0xff,
    0x31, 0xc0,
    0x0f, 0x05,
    'U', '1', ' '
};

static const uint8_t user_yield_code2[] = {
    0x41, 0xbc, 0x03, 0x00, 0x00, 0x00,
    0x48, 0x8d, 0x35, 0x23, 0x00, 0x00, 0x00,
    0xbf, 0x01, 0x00, 0x00, 0x00,
    0xba, 0x03, 0x00, 0x00, 0x00,
    0xb8, 0x01, 0x00, 0x00, 0x00,
    0x0f, 0x05,
    0xb8, 0x02, 0x00, 0x00, 0x00,
    0x0f, 0x05,
    0x41, 0xff, 0xcc,
    0x75, 0xdc,
    0x31, 0xff,
    0x31, 0xc0,
    0x0f, 0x05,
    'U', '2', ' '
};

static const uint8_t user_fault_code[] = {
    0x0f, 0x0b  /* ud2 */
};

/*
 * Proto 7: User Mode and System Calls tests
 */
void test_user(void) {
    serial_puts("\n=== PROTO7 TESTS (User Mode) ===\n");

    /* Test 1: Hello from user mode */
    serial_puts("PROTO7 TEST1: Hello from user mode\n");
    task_t *user_task1 = task_create_user(user_hello_code, sizeof(user_hello_code));
    scheduler_add(user_task1);
    while (user_task1->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO7 TEST1: Complete\n");

    /* Test 2: User yield test (two user tasks alternating) */
    serial_puts("PROTO7 TEST2: User yield test\n");
    task_t *u1 = task_create_user(user_yield_code1, sizeof(user_yield_code1));
    task_t *u2 = task_create_user(user_yield_code2, sizeof(user_yield_code2));
    scheduler_add(u1);
    scheduler_add(u2);
    while (u1->state != TASK_FINISHED || u2->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("\nPROTO7 TEST2: Complete\n");

    /* Test 3: Fault isolation (user fault doesn't crash kernel) */
    serial_puts("PROTO7 TEST3: Fault isolation\n");
    task_t *fault_task = task_create_user(user_fault_code, sizeof(user_fault_code));
    scheduler_add(fault_task);
    while (fault_task->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO7 TEST3: Kernel survived\n");
}

static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

/*
 * Proto 8: ELF64 Loader tests
 */
void test_elf(void) {
    serial_puts("\n=== PROTO8 TESTS (ELF Loader) ===\n");

    /* Check if modules are available */
    if (limine_modules == NULL || limine_modules->module_count == 0) {
        serial_puts("PROTO8: No modules loaded, skipping ELF tests\n");
        return;
    }

    serial_puts("PROTO8: Found ");
    print_hex(limine_modules->module_count);
    serial_puts(" modules\n");

    /* List modules */
    for (uint64_t i = 0; i < limine_modules->module_count; i++) {
        struct limine_file *mod = limine_modules->modules[i];
        serial_puts("  Module: ");
        serial_puts(mod->path);
        serial_puts(" (");
        print_hex(mod->size);
        serial_puts(" bytes)\n");
    }

    /* Test 1: Run init.elf that prints and exits */
    serial_puts("PROTO8 TEST1: ELF hello world\n");
    struct limine_file *init_mod = find_module("init.elf");
    if (init_mod != NULL) {
        task_t *init_task = task_create_elf(init_mod->address, init_mod->size);
        if (init_task != NULL) {
            scheduler_add(init_task);
            while (init_task->state != TASK_FINISHED) {
                task_yield();
            }
            serial_puts("PROTO8 TEST1: Complete\n");
        } else {
            serial_puts("PROTO8 TEST1: Failed to create task\n");
        }
    } else {
        serial_puts("PROTO8 TEST1: init.elf not found\n");
    }

    /* Test 2: Two ELF user programs yielding */
    serial_puts("PROTO8 TEST2: ELF yield test\n");
    struct limine_file *yield1_mod = find_module("yield1.elf");
    struct limine_file *yield2_mod = find_module("yield2.elf");
    if (yield1_mod != NULL && yield2_mod != NULL) {
        task_t *elf_y1 = task_create_elf(yield1_mod->address, yield1_mod->size);
        task_t *elf_y2 = task_create_elf(yield2_mod->address, yield2_mod->size);
        if (elf_y1 != NULL && elf_y2 != NULL) {
            scheduler_add(elf_y1);
            scheduler_add(elf_y2);
            while (elf_y1->state != TASK_FINISHED || elf_y2->state != TASK_FINISHED) {
                task_yield();
            }
            serial_puts("\nPROTO8 TEST2: Complete\n");
        } else {
            serial_puts("PROTO8 TEST2: Failed to create tasks\n");
        }
    } else {
        serial_puts("PROTO8 TEST2: yield ELFs not found\n");
    }

    /* Test 3: Privilege separation - user tries to access kernel memory */
    serial_puts("PROTO8 TEST3: Privilege separation\n");
    struct limine_file *fault_mod = find_module("fault.elf");
    if (fault_mod != NULL) {
        task_t *elf_fault = task_create_elf(fault_mod->address, fault_mod->size);
        if (elf_fault != NULL) {
            scheduler_add(elf_fault);
            while (elf_fault->state != TASK_FINISHED) {
                task_yield();
            }
            serial_puts("PROTO8 TEST3: Kernel survived, privilege separation works\n");
        } else {
            serial_puts("PROTO8 TEST3: Failed to create task\n");
        }
    } else {
        serial_puts("PROTO8 TEST3: fault.elf not found\n");
    }
}

/*
 * Proto 9: Filesystem and Disk-Backed Loading tests
 */
void test_filesystem(void) {
    serial_puts("\n=== PROTO9 TESTS (Filesystem) ===\n");

    /* Test 1: Load and run INIT.ELF from disk */
    serial_puts("PROTO9 TEST1: Load init.elf from disk\n");
    task_t *disk_init = task_create_from_path("INIT.ELF");
    if (disk_init != NULL) {
        scheduler_add(disk_init);
        while (disk_init->state != TASK_FINISHED) {
            task_yield();
        }
        serial_puts("PROTO9 TEST1: Complete\n");
    } else {
        serial_puts("PROTO9 TEST1: Failed to load INIT.ELF\n");
    }

    /* Test 2: Load and run YIELD1.ELF + YIELD2.ELF from disk */
    serial_puts("PROTO9 TEST2: Load yield1/yield2 from disk\n");
    task_t *disk_y1 = task_create_from_path("YIELD1.ELF");
    task_t *disk_y2 = task_create_from_path("YIELD2.ELF");
    if (disk_y1 != NULL && disk_y2 != NULL) {
        scheduler_add(disk_y1);
        scheduler_add(disk_y2);
        while (disk_y1->state != TASK_FINISHED || disk_y2->state != TASK_FINISHED) {
            task_yield();
        }
        serial_puts("\nPROTO9 TEST2: Complete\n");
    } else {
        serial_puts("PROTO9 TEST2: Failed to load yield ELFs\n");
    }

    /* Test 3: Verify nonexistent file returns NULL */
    serial_puts("PROTO9 TEST3: Nonexistent file test\n");
    task_t *nofile = task_create_from_path("NOFILE.ELF");
    if (nofile == NULL) {
        serial_puts("PROTO9 TEST3: Correctly returned NULL for missing file\n");
    } else {
        serial_puts("PROTO9 TEST3: ERROR - should have returned NULL\n");
    }
}
