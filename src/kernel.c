#include <stdint.h>
#include <stddef.h>
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "serial.h"
#include "panic.h"
#include "hhdm.h"
#include "idt.h"
#include "pmm.h"

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

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

uint64_t hhdm_offset;

/* Global Limine response pointers for PMM */
struct limine_memmap_response *limine_memmap;
struct limine_executable_address_response *limine_exec_addr;

static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static volatile uint64_t test_global = 0xDEADBEEF;

void panic(const char *msg) {
    serial_puts("PANIC: ");
    serial_puts(msg);
    serial_puts("\n");
    for (;;) {
        asm volatile("cli; hlt");
    }
}

void hhdm_init(uint64_t offset) {
    hhdm_offset = offset;
}

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

    /* Test triggers (activated via -DTEST_UD or -DTEST_PF) */
#if defined(TEST_UD)
    serial_puts("Testing: triggering #UD (invalid opcode)...\n");
    asm volatile("ud2");
#elif defined(TEST_PF)
    serial_puts("Testing: triggering #PF (page fault)...\n");
    *(volatile uint64_t *)0xdeadbeefdeadbeef = 1;
#endif

    serial_puts("cool-os: entering idle loop\n");
    for (;;) {
        asm volatile("hlt");
    }
}
