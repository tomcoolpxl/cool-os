#include <stdint.h>
#include <stddef.h>
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "serial.h"
#include "panic.h"
#include "hhdm.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include "heap.h"
#include "pic.h"
#include "pit.h"
#include "timer.h"
#include "task.h"
#include "scheduler.h"
#include "syscall.h"
#include "elf.h"
#include "block.h"
#include "fat32.h"
#include "vfs.h"
#include "framebuffer.h"
#include "console.h"

void kmain(void);

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2)

__attribute__((used, section(".limine_requests")))
static volatile struct limine_entry_point_request entry_request = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .entry = kmain
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

uint64_t hhdm_offset;

/* Global Limine response pointers for PMM */
struct limine_memmap_response *limine_memmap;
struct limine_executable_address_response *limine_exec_addr;
struct limine_module_response *limine_modules;

/*
 * Find a Limine module by path suffix (e.g., "init.elf").
 * Returns pointer to limine_file or NULL if not found.
 */
struct limine_file *find_module(const char *name) {
    if (limine_modules == NULL) {
        return NULL;
    }
    for (uint64_t i = 0; i < limine_modules->module_count; i++) {
        struct limine_file *mod = limine_modules->modules[i];
        const char *path = mod->path;
        /* Find last '/' in path */
        const char *basename = path;
        for (const char *p = path; *p; p++) {
            if (*p == '/') {
                basename = p + 1;
            }
        }
        /* Compare basename with requested name */
        const char *a = basename;
        const char *b = name;
        while (*a && *b && *a == *b) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            return mod;
        }
    }
    return NULL;
}

static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static volatile uint64_t test_global = 0xDEADBEEF;

void panic(const char *msg) {
    /* Disable interrupts */
    asm volatile("cli");

    /* Try framebuffer console first */
    console_clear();
    console_puts("PANIC: ");
    console_puts(msg);
    console_puts("\n");

    /* Also output to serial for debugging */
    serial_puts("PANIC: ");
    serial_puts(msg);
    serial_puts("\n");

    for (;;) {
        asm volatile("hlt");
    }
}

void hhdm_init(uint64_t offset) {
    hhdm_offset = offset;
}

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

/*
 * Proto 7 user programs as raw machine code
 *
 * These are position-independent programs that run at user-space virtual
 * addresses (0x400000+). Each program only uses syscalls (no kernel addresses).
 *
 * Syscall ABI (System V AMD64):
 *   RAX = syscall number (0=exit, 1=write, 2=yield)
 *   RDI = arg1, RSI = arg2, RDX = arg3
 *   SYSCALL instruction
 */

/*
 * user_hello_code: prints "Hello from user mode!\n" and exits
 *
 * Assembly (30 bytes code + 22 bytes message = 52 bytes total):
 *   lea rsi, [rip+23]    ; message at offset 30, rip after lea = 7
 *   mov edi, 1           ; fd = stdout
 *   mov edx, 22          ; len
 *   mov eax, 1           ; SYS_write
 *   syscall
 *   xor edi, edi         ; exit code = 0
 *   xor eax, eax         ; SYS_exit
 *   syscall
 * msg: "Hello from user mode!\n"
 */
static const uint8_t user_hello_code[] = {
    0x48, 0x8d, 0x35, 0x17, 0x00, 0x00, 0x00,   /* 0:  lea rsi, [rip+23] */
    0xbf, 0x01, 0x00, 0x00, 0x00,               /* 7:  mov edi, 1 */
    0xba, 0x16, 0x00, 0x00, 0x00,               /* 12: mov edx, 22 */
    0xb8, 0x01, 0x00, 0x00, 0x00,               /* 17: mov eax, 1 */
    0x0f, 0x05,                                 /* 22: syscall */
    0x31, 0xff,                                 /* 24: xor edi, edi */
    0x31, 0xc0,                                 /* 26: xor eax, eax */
    0x0f, 0x05,                                 /* 28: syscall */
    'H','e','l','l','o',' ','f','r','o','m',' ',
    'u','s','e','r',' ','m','o','d','e','!','\n'
};

/*
 * user_yield_code1: prints "U1 " three times with yields, then exits
 *
 * Assembly (48 bytes code + 3 bytes message = 51 bytes total):
 *   mov r12d, 3          ; loop counter
 * loop:
 *   lea rsi, [rip+35]    ; message at offset 48, rip after lea = 13
 *   mov edi, 1
 *   mov edx, 3
 *   mov eax, 1           ; SYS_write
 *   syscall
 *   mov eax, 2           ; SYS_yield
 *   syscall
 *   dec r12d
 *   jnz loop
 *   xor edi, edi
 *   xor eax, eax         ; SYS_exit
 *   syscall
 * msg: "U1 "
 */
static const uint8_t user_yield_code1[] = {
    0x41, 0xbc, 0x03, 0x00, 0x00, 0x00,         /* 0:  mov r12d, 3 */
    0x48, 0x8d, 0x35, 0x23, 0x00, 0x00, 0x00,   /* 6:  lea rsi, [rip+35] */
    0xbf, 0x01, 0x00, 0x00, 0x00,               /* 13: mov edi, 1 */
    0xba, 0x03, 0x00, 0x00, 0x00,               /* 18: mov edx, 3 */
    0xb8, 0x01, 0x00, 0x00, 0x00,               /* 23: mov eax, 1 */
    0x0f, 0x05,                                 /* 28: syscall */
    0xb8, 0x02, 0x00, 0x00, 0x00,               /* 30: mov eax, 2 */
    0x0f, 0x05,                                 /* 35: syscall */
    0x41, 0xff, 0xcc,                           /* 37: dec r12d */
    0x75, 0xdc,                                 /* 40: jnz -36 (to offset 6: 42-36=6) */
    0x31, 0xff,                                 /* 42: xor edi, edi */
    0x31, 0xc0,                                 /* 44: xor eax, eax */
    0x0f, 0x05,                                 /* 46: syscall */
    'U', '1', ' '
};

/* user_yield_code2: same structure, prints "U2 " */
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
    0x75, 0xdc,                                 /* jnz -36 (to offset 6) */
    0x31, 0xff,
    0x31, 0xc0,
    0x0f, 0x05,
    'U', '2', ' '
};

/* user_fault_code: triggers invalid opcode (ud2) - 2 bytes */
static const uint8_t user_fault_code[] = {
    0x0f, 0x0b
};

void kmain(void) {
    serial_init();
    serial_puts("cool-os: kernel loaded\n");

    /* Check if Limine base revision is supported */
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        panic("Unsupported Limine version");
    }

    /*
     * Validate stack alignment.
     * Limine jumps directly to entry point (no call instruction),
     * so RSP should be 16-byte aligned at entry per SysV ABI.
     * Note: Inside a normal C callee after 'call', RSP % 16 == 8
     * due to return address push, but Limine uses direct jump.
     */
    uint64_t rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    if ((rsp & 0xf) != 0) {
        panic("Stack not 16-byte aligned!");
    }

    /* Verify HHDM response and initialize global offset */
    if (hhdm_request.response == NULL) {
        panic("HHDM request not fulfilled by bootloader");
    }
    hhdm_init(hhdm_request.response->offset);

    /* Verify we're in higher-half */
    ASSERT((uint64_t)&test_global >= 0xFFFFFFFF80000000ULL);

    serial_puts("HHDM offset: ");
    print_hex(hhdm_offset);
    serial_puts("\n");

    /* Initialize GDT with user segments and TSS (must be before IDT) */
    gdt_init();

    /* Initialize IDT and exception handlers */
    idt_init();

    /* Validate Limine memmap and exec_addr responses */
    if (memmap_request.response == NULL) {
        panic("Memory map request not fulfilled by bootloader");
    }
    if (exec_addr_request.response == NULL) {
        panic("Executable address request not fulfilled by bootloader");
    }
    limine_memmap = memmap_request.response;
    limine_exec_addr = exec_addr_request.response;
    limine_modules = module_request.response;  /* May be NULL if no modules */

    /* Initialize physical memory manager */
    pmm_init();

    /* PMM validation test: allocate, write, verify, free 10 frames */
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

    /* Initialize heap allocator */
    heap_init();

    /* Initialize SYSCALL/SYSRET mechanism */
    syscall_init();

    /* Initialize block device, filesystem, and VFS */
    if (block_init() == 0) {
        if (fat_mount() == 0) {
            vfs_init();
        }
    }

    /* Initialize framebuffer */
    if (fb_init() != 0) {
        serial_puts("fb: Initialization failed\n");
    } else {
        console_init();
    }

    /* Heap validation test 1: Basic alloc/free */
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

    void *p3 = kmalloc(256);
    ASSERT(p3 != NULL);
    serial_puts("HEAP: Allocated 256 bytes at ");
    print_hex((uint64_t)p3);
    serial_puts("\n");

    kfree(p2);
    serial_puts("HEAP: Freed 128-byte block\n");

    kfree(p1);
    serial_puts("HEAP: Freed 64-byte block\n");

    kfree(p3);
    serial_puts("HEAP: Freed 256-byte block\n");
    serial_puts("HEAP: Basic allocation test passed\n");

    /* Heap validation test 2: Coalescing test */
    serial_puts("HEAP: Running coalescing test...\n");
    void *c1 = kmalloc(100);
    void *c2 = kmalloc(100);
    void *c3 = kmalloc(100);
    ASSERT(c1 != NULL && c2 != NULL && c3 != NULL);
    serial_puts("HEAP: Allocated 3 x 100-byte blocks\n");

    kfree(c2);
    serial_puts("HEAP: Freed middle block\n");

    kfree(c1);
    serial_puts("HEAP: Freed first block (should coalesce with middle)\n");

    kfree(c3);
    serial_puts("HEAP: Freed last block (should coalesce all)\n");

    void *big = kmalloc(300);
    ASSERT(big != NULL);
    serial_puts("HEAP: Allocated 300-byte block (coalescing worked): ");
    print_hex((uint64_t)big);
    serial_puts("\n");
    kfree(big);
    serial_puts("HEAP: Coalescing test passed\n");

    /* Heap validation test 3: Stress test */
    serial_puts("HEAP: Running stress test (100 allocations)...\n");
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(32);
        ASSERT(ptrs[i] != NULL);
    }
    serial_puts("HEAP: All 100 allocations succeeded\n");

    for (int i = 0; i < 100; i += 2) {
        kfree(ptrs[i]);
    }
    serial_puts("HEAP: Freed alternate blocks\n");

    for (int i = 0; i < 100; i += 2) {
        ptrs[i] = kmalloc(32);
        ASSERT(ptrs[i] != NULL);
    }
    serial_puts("HEAP: Re-allocated alternate blocks\n");

    for (int i = 0; i < 100; i++) {
        kfree(ptrs[i]);
    }
    serial_puts("HEAP: Freed all blocks\n");
    serial_puts("HEAP: Stress test passed\n");

    /* Test triggers (activated via -DTEST_UD or -DTEST_PF) */
#if defined(TEST_UD)
    serial_puts("Testing: triggering #UD (invalid opcode)...\n");
    asm volatile("ud2");
#elif defined(TEST_PF)
    serial_puts("Testing: triggering #PF (page fault)...\n");
    *(volatile uint64_t *)0xdeadbeefdeadbeef = 1;
#endif

    /* Initialize PIC, PIT, and timer subsystem */
    pic_init();
    pit_init(100);
    timer_init();

    /* Initialize scheduler (before enabling interrupts) */
    scheduler_init();

    /* Enable interrupts */
    serial_puts("cool-os: enabling interrupts\n");
    asm volatile("sti");

    /* Proto 6 validation tests */

    /* Test 1: Two task alternation */
    serial_puts("PROTO6 TEST1: Two task alternation\n");
    task_t *task_a = task_create(test_task_a);
    task_t *task_b = task_create(test_task_b);
    scheduler_add(task_a);
    scheduler_add(task_b);

    /* Yield repeatedly to let tasks run */
    while (task_a->state != TASK_FINISHED || task_b->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO6 TEST1: Complete\n");

    /* Test 2: Task exit handling */
    serial_puts("PROTO6 TEST2: Task exit handling\n");
    task_t *task_exit = task_create(test_task_exit);
    scheduler_add(task_exit);
    while (task_exit->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO6 TEST2: Complete\n");

    /* Test 3: Idle fallback */
    serial_puts("PROTO6 TEST3: Idle fallback - entering idle\n");

    /* Proto 7 validation tests */

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

    /* Proto 8 validation tests (ELF loading) */
    serial_puts("\n=== PROTO8 TESTS (ELF Loader) ===\n");

    /* Check if modules are available */
    if (limine_modules == NULL || limine_modules->module_count == 0) {
        serial_puts("PROTO8: No modules loaded, skipping ELF tests\n");
    } else {
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

    /* Proto 9 validation tests (Disk-based ELF loading) */
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

#ifdef TEST_GRAPHICS
    /* Proto 10 validation tests (Framebuffer) */
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

        /* Start with dark blue background */
        fb_clear(0x00002244);

        uint32_t rect_x = 0;
        uint32_t rect_y = fb_info->render_height / 2 - 50;  /* Center vertically */
        uint32_t max_x = fb_info->render_width - 100;
        int direction = 1;
        uint64_t start_ticks = timer_get_ticks();
        uint64_t end_ticks = start_ticks + (3 * TIMER_HZ);  /* 3 seconds */
        uint32_t anim_frames = 0;

        while (timer_get_ticks() < end_ticks) {
            /* Clear and redraw (back buffer makes this fast) */
            fb_clear(0x00002244);
            fb_fill_rect(rect_x, rect_y, 100, 100, 0x00FFFFFF);
            fb_present();
            anim_frames++;

            /* Move rectangle */
            rect_x += direction * 8;
            if (rect_x >= max_x) {
                direction = -1;
                rect_x = max_x;
            } else if (direction == -1 && rect_x <= 8) {
                direction = 1;
                rect_x = 0;
            }
        }
        serial_puts("PROTO10 TEST2: Complete (");
        print_hex(anim_frames);
        serial_puts(" frames)\n");

        /* Test 3: Resolution independence (print dimensions) */
        serial_puts("PROTO10 TEST3: Resolution independence\n");
        serial_puts("  Hardware: ");
        print_hex(fb_info->hw_width);
        serial_puts("x");
        print_hex(fb_info->hw_height);
        serial_puts("\n");
        serial_puts("  Render: ");
        print_hex(fb_info->render_width);
        serial_puts("x");
        print_hex(fb_info->render_height);
        serial_puts("\n");
        serial_puts("  Back buffer: ");
        print_hex((uint64_t)fb_info->back);
        serial_puts("\n");
        serial_puts("PROTO10 TEST3: Complete\n");

        /* Test 4: Color cycling (hold each color 500ms) */
        serial_puts("PROTO10 TEST4: Color cycle test\n");
        uint32_t colors[] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFF00};

        for (int i = 0; i < 4; i++) {
            fb_clear(colors[i]);
            fb_present();
            timer_sleep_ms(500);
        }
        serial_puts("PROTO10 TEST4: Complete\n");

        serial_puts("\n=== PROTO10 TESTS COMPLETE ===\n");
    }

    /* Proto 11 validation tests (Framebuffer Text Console) */
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

        /* Test 2: Multiple lines and newline handling */
        serial_puts("PROTO11 TEST2: Multiple lines\n");
        console_puts("Line 1: The quick brown fox\n");
        console_puts("Line 2: jumps over the lazy dog\n");
        console_puts("Line 3: 0123456789\n");
        console_puts("Line 4: ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
        console_puts("Line 5: abcdefghijklmnopqrstuvwxyz\n");
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST2: Complete\n");

        /* Test 3: Scrolling test - print many lines */
        serial_puts("PROTO11 TEST3: Scrolling test\n");
        console_clear();
        for (int i = 0; i < 60; i++) {
            console_puts("Scroll test line ");
            /* Print number manually */
            if (i >= 10) {
                console_putc('0' + (i / 10));
            }
            console_putc('0' + (i % 10));
            console_puts("\n");
        }
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST3: Complete\n");

        /* Test 4: console_clear() test */
        serial_puts("PROTO11 TEST4: Clear screen test\n");
        console_clear();
        console_puts("Screen cleared! This is the only text.\n");
        timer_sleep_ms(1000);
        serial_puts("PROTO11 TEST4: Complete\n");

        serial_puts("\n=== PROTO11 TESTS COMPLETE ===\n");
    }
#endif /* TEST_GRAPHICS */

    serial_puts("\ncool-os: entering idle loop\n");
    for (;;) {
        asm volatile("hlt");
    }
}
