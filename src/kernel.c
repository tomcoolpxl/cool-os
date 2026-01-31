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
#include "kbd.h"
#include "pci.h"
#include "lapic.h"
#include "shell.h"

#ifdef REGTEST_BUILD
#include "regtest.h"
#endif

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

static void print_dec(uint64_t val) {
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[21];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

/* Helper to output string to both serial and console */
static void puts_both(const char *s) {
    serial_puts(s);
    if (fb_get_info() != NULL) {
        console_puts(s);
    }
}

/* Helper to output decimal number to both serial and console */
static void putdec_both(uint64_t val) {
    if (val == 0) {
        serial_putc('0');
        if (fb_get_info() != NULL) console_putc('0');
        return;
    }
    char buf[21];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        char c = buf[--i];
        serial_putc(c);
        if (fb_get_info() != NULL) console_putc(c);
    }
}

static void print_kernel_info(void) {
    /* Clear console first for clean display */
    if (fb_get_info() != NULL) {
        console_clear();
    }

    puts_both("I am coolOS.\n\n");
    puts_both("========================================\n");
    puts_both("  cool-os v0.13 (Proto 13)\n");
    puts_both("  x86-64 Teaching Kernel\n");
    puts_both("========================================\n");

    /* Memory info */
    uint64_t free_pages = pmm_get_free_frames();
    uint64_t free_kb = free_pages * 4;
    puts_both("Memory: ");
    putdec_both(free_kb);
    puts_both(" KB free (");
    putdec_both(free_pages);
    puts_both(" pages)\n");

    /* Display info */
    if (fb_get_info() != NULL) {
        const framebuffer_t *fbi = fb_get_info();
        puts_both("Display: ");
        putdec_both(fbi->render_width);
        puts_both("x");
        putdec_both(fbi->render_height);
        puts_both(" (");
        putdec_both(fbi->hw_width);
        puts_both("x");
        putdec_both(fbi->hw_height);
        puts_both(" native)\n");
    }

    puts_both("========================================\n\n");

    /* Make sure it's visible on screen */
    if (fb_get_info() != NULL) {
        fb_present();
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

/* External test runner for TEST_BUILD */
#ifdef TEST_BUILD
extern void run_kernel_tests(void);
#endif

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

    /* Test triggers (activated via -DTEST_UD or -DTEST_PF) */
#if defined(TEST_UD)
    serial_puts("Testing: triggering #UD (invalid opcode)...\n");
    asm volatile("ud2");
#elif defined(TEST_PF)
    serial_puts("Testing: triggering #PF (page fault)...\n");
    *(volatile uint64_t *)0xdeadbeefdeadbeef = 1;
#endif

    /* Initialize PCI Bus (Experimental XHCI disabled for stability) */
    /* pci_init(); */

    /* Initialize PIC, PIT, and timer subsystem */
    pic_init();
    pit_init(100);
    timer_init();
    
    /* Enable Local APIC for MSI support */
    lapic_init();

    /* Initialize keyboard driver (after PIC so IRQ1 unmask works) */
    kbd_init();

    /* Initialize scheduler (before enabling interrupts) */
    scheduler_init();

    /* Enable interrupts */
    serial_puts("cool-os: enabling interrupts\n");
    asm volatile("sti");

    /* Print kernel info to both console and serial */
    print_kernel_info();

#ifdef TEST_BUILD
    /* Run interactive tests from tests/kernel_tests.c */
    run_kernel_tests();
#endif

#ifdef REGTEST_BUILD
    /* Run regression tests and exit QEMU with appropriate code */
    serial_puts("\ncool-os: starting regression tests\n");
    int regtest_result = regtest_run_all();
    regtest_exit(regtest_result == 0);
    /* Never reached - QEMU exits via isa-debug-exit */
#endif

    /* Initialize and start the kernel shell */
    shell_init();

    /* Enter scheduler loop - kmain becomes idle when no work to do */
    serial_puts("cool-os: entering scheduler\n");
    for (;;) {
        scheduler_yield();
    }
}
