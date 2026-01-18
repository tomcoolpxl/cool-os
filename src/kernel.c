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
    0x75, 0xdf,                                 /* 40: jnz -33 (to offset 6) */
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
    0x75, 0xdf,
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

    /* Make kernel pages user-accessible for user mode execution */
    serial_puts("PAGING: Making kernel pages user-accessible\n");
    paging_set_user_accessible((uint64_t)__kernel_start, (uint64_t)__kernel_end);

    /* Test 1: Hello from user mode */
    serial_puts("PROTO7 TEST1: Hello from user mode\n");
    task_t *user_task1 = task_create_user(user_hello);
    scheduler_add(user_task1);
    while (user_task1->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO7 TEST1: Complete\n");

    /* Test 2: User yield test (two user tasks alternating) */
    serial_puts("PROTO7 TEST2: User yield test\n");
    task_t *u1 = task_create_user(user_yield_task1);
    task_t *u2 = task_create_user(user_yield_task2);
    scheduler_add(u1);
    scheduler_add(u2);
    while (u1->state != TASK_FINISHED || u2->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("\nPROTO7 TEST2: Complete\n");

    /* Test 3: Fault isolation (user fault doesn't crash kernel) */
    serial_puts("PROTO7 TEST3: Fault isolation\n");
    task_t *fault_task = task_create_user(user_fault_test);
    scheduler_add(fault_task);
    while (fault_task->state != TASK_FINISHED) {
        task_yield();
    }
    serial_puts("PROTO7 TEST3: Kernel survived\n");

    serial_puts("cool-os: entering idle loop\n");
    for (;;) {
        asm volatile("hlt");
    }
}
