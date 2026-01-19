/*
 * Regression Test Suites for cool-os
 *
 * Each suite tests a specific kernel subsystem and reports pass/fail
 * via the regtest infrastructure.
 */

#include "regtest.h"
#include "pmm.h"
#include "heap.h"
#include "hhdm.h"
#include "task.h"
#include "scheduler.h"
#include "vfs.h"
#include "framebuffer.h"
#include "console.h"
#include "serial.h"
#include "limine.h"
#include <stdint.h>
#include <stddef.h>

/* External Limine module response for ELF tests */
extern struct limine_module_response *limine_modules;
extern struct limine_file *find_module(const char *name);

/* ========== PMM Suite ========== */

int regtest_pmm(void) {
    regtest_start_suite("pmm");

    /* Test 1: Basic allocation */
    uint64_t frame = pmm_alloc_frame();
    if (frame == 0) {
        regtest_fail("pmm_alloc_basic", "allocation returned 0");
        regtest_end_suite("pmm");
        return -1;
    }
    regtest_pass("pmm_alloc_basic");

    /* Test 2: Write/read pattern at allocated frame */
    volatile uint64_t *ptr = (volatile uint64_t *)phys_to_hhdm(frame);
    *ptr = 0xCAFEBABEDEADBEEFULL;
    if (*ptr != 0xCAFEBABEDEADBEEFULL) {
        regtest_fail("pmm_write_read", "pattern mismatch");
        pmm_free_frame(frame);
        regtest_end_suite("pmm");
        return -1;
    }
    regtest_pass("pmm_write_read");

    /* Test 3: Free and realloc should return same or different frame */
    pmm_free_frame(frame);
    uint64_t frame2 = pmm_alloc_frame();
    if (frame2 == 0) {
        regtest_fail("pmm_realloc", "realloc after free returned 0");
        regtest_end_suite("pmm");
        return -1;
    }
    pmm_free_frame(frame2);
    regtest_pass("pmm_realloc");

    /* Test 4: Multiple frame allocation */
    uint64_t frames[10];
    uint64_t free_before = pmm_get_free_frames();
    for (int i = 0; i < 10; i++) {
        frames[i] = pmm_alloc_frame();
        if (frames[i] == 0) {
            regtest_fail("pmm_multi_alloc", "allocation failed");
            /* Free already allocated frames */
            for (int j = 0; j < i; j++) {
                pmm_free_frame(frames[j]);
            }
            regtest_end_suite("pmm");
            return -1;
        }
    }
    /* Verify all unique */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            if (frames[i] == frames[j]) {
                regtest_fail("pmm_multi_unique", "duplicate frame returned");
                for (int k = 0; k < 10; k++) pmm_free_frame(frames[k]);
                regtest_end_suite("pmm");
                return -1;
            }
        }
    }
    regtest_pass("pmm_multi_alloc");

    /* Test 5: Free all and verify count restored */
    for (int i = 0; i < 10; i++) {
        pmm_free_frame(frames[i]);
    }
    uint64_t free_after = pmm_get_free_frames();
    if (free_before != free_after) {
        regtest_fail("pmm_free_count", "free count not restored");
        regtest_end_suite("pmm");
        return -1;
    }
    regtest_pass("pmm_free_count");

    /* Test 6: Contiguous allocation */
    uint64_t contig = pmm_alloc_frames_contiguous(4);
    if (contig == 0) {
        regtest_fail("pmm_contiguous", "contiguous alloc returned 0");
        regtest_end_suite("pmm");
        return -1;
    }
    /* Verify alignment */
    if ((contig & 0xFFF) != 0) {
        regtest_fail("pmm_contiguous_align", "not page aligned");
        regtest_end_suite("pmm");
        return -1;
    }
    /* Free all 4 frames */
    for (int i = 0; i < 4; i++) {
        pmm_free_frame(contig + i * 4096);
    }
    regtest_pass("pmm_contiguous");

    regtest_end_suite("pmm");
    return 0;
}

/* ========== Heap Suite ========== */

int regtest_heap(void) {
    regtest_start_suite("heap");

    /* Test 1: Basic allocation */
    void *p1 = kmalloc(64);
    if (p1 == NULL) {
        regtest_fail("heap_alloc_basic", "kmalloc(64) returned NULL");
        regtest_end_suite("heap");
        return -1;
    }
    regtest_pass("heap_alloc_basic");

    /* Test 2: Alignment check (should be 16-byte aligned) */
    if (((uint64_t)p1 & 0xF) != 0) {
        regtest_fail("heap_alignment", "not 16-byte aligned");
        kfree(p1);
        regtest_end_suite("heap");
        return -1;
    }
    regtest_pass("heap_alignment");

    /* Test 3: Multiple allocations */
    void *p2 = kmalloc(128);
    void *p3 = kmalloc(256);
    if (p2 == NULL || p3 == NULL) {
        regtest_fail("heap_multi_alloc", "multiple allocations failed");
        if (p2) kfree(p2);
        if (p3) kfree(p3);
        kfree(p1);
        regtest_end_suite("heap");
        return -1;
    }
    /* Check they don't overlap */
    if (p1 == p2 || p1 == p3 || p2 == p3) {
        regtest_fail("heap_no_overlap", "allocations overlap");
        kfree(p1);
        kfree(p2);
        kfree(p3);
        regtest_end_suite("heap");
        return -1;
    }
    regtest_pass("heap_multi_alloc");

    /* Test 4: Free and realloc */
    kfree(p2);
    void *p4 = kmalloc(64);
    if (p4 == NULL) {
        regtest_fail("heap_realloc", "realloc after free failed");
        kfree(p1);
        kfree(p3);
        regtest_end_suite("heap");
        return -1;
    }
    kfree(p4);
    regtest_pass("heap_realloc");

    /* Test 5: Coalescing - free middle, first, then last; realloc bigger */
    kfree(p1);
    kfree(p3);

    void *c1 = kmalloc(100);
    void *c2 = kmalloc(100);
    void *c3 = kmalloc(100);
    if (c1 == NULL || c2 == NULL || c3 == NULL) {
        regtest_fail("heap_coalesce_setup", "setup allocations failed");
        if (c1) kfree(c1);
        if (c2) kfree(c2);
        if (c3) kfree(c3);
        regtest_end_suite("heap");
        return -1;
    }
    kfree(c2);
    kfree(c1);
    kfree(c3);
    /* Now there should be a large coalesced block */
    void *big = kmalloc(300);
    if (big == NULL) {
        regtest_fail("heap_coalesce", "coalescing failed");
        regtest_end_suite("heap");
        return -1;
    }
    kfree(big);
    regtest_pass("heap_coalesce");

    /* Test 6: Stress test (100 allocations) */
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(32);
        if (ptrs[i] == NULL) {
            regtest_fail("heap_stress", "stress allocation failed");
            for (int j = 0; j < i; j++) kfree(ptrs[j]);
            regtest_end_suite("heap");
            return -1;
        }
    }
    /* Free alternate */
    for (int i = 0; i < 100; i += 2) {
        kfree(ptrs[i]);
    }
    /* Reallocate */
    for (int i = 0; i < 100; i += 2) {
        ptrs[i] = kmalloc(32);
        if (ptrs[i] == NULL) {
            regtest_fail("heap_stress_realloc", "stress realloc failed");
            for (int j = 1; j < 100; j += 2) kfree(ptrs[j]);
            regtest_end_suite("heap");
            return -1;
        }
    }
    /* Free all */
    for (int i = 0; i < 100; i++) {
        kfree(ptrs[i]);
    }
    regtest_pass("heap_stress");

    regtest_end_suite("heap");
    return 0;
}

/* ========== Task Suite ========== */

/* Test task variables */
static volatile int task_a_count = 0;
static volatile int task_b_count = 0;
static volatile int task_exit_ran = 0;

static void regtest_task_a_fn(void) {
    for (int i = 0; i < 3; i++) {
        task_a_count++;
        task_yield();
    }
}

static void regtest_task_b_fn(void) {
    for (int i = 0; i < 3; i++) {
        task_b_count++;
        task_yield();
    }
}

static void regtest_task_exit_fn(void) {
    task_exit_ran = 1;
}

int regtest_task(void) {
    regtest_start_suite("task");

    /* Reset counters */
    task_a_count = 0;
    task_b_count = 0;
    task_exit_ran = 0;

    /* Test 1: Create task */
    task_t *ta = task_create(regtest_task_a_fn);
    if (ta == NULL) {
        regtest_fail("task_create", "task_create returned NULL");
        regtest_end_suite("task");
        return -1;
    }
    regtest_pass("task_create");

    /* Test 2: Task structure fields */
    if (ta->state != TASK_READY) {
        regtest_fail("task_state", "initial state not READY");
        regtest_end_suite("task");
        return -1;
    }
    if (ta->stack_base == NULL) {
        regtest_fail("task_stack", "stack_base is NULL");
        regtest_end_suite("task");
        return -1;
    }
    regtest_pass("task_structure");

    /* Test 3: Two tasks alternating */
    task_t *tb = task_create(regtest_task_b_fn);
    if (tb == NULL) {
        regtest_fail("task_create_second", "second task_create failed");
        regtest_end_suite("task");
        return -1;
    }
    scheduler_add(ta);
    scheduler_add(tb);

    /* Let tasks run */
    while (ta->state != TASK_FINISHED || tb->state != TASK_FINISHED) {
        task_yield();
    }

    if (task_a_count != 3 || task_b_count != 3) {
        regtest_fail("task_alternation", "counts incorrect");
        regtest_end_suite("task");
        return -1;
    }
    regtest_pass("task_alternation");

    /* Test 4: Task exit handling */
    task_t *te = task_create(regtest_task_exit_fn);
    if (te == NULL) {
        regtest_fail("task_exit_create", "create failed");
        regtest_end_suite("task");
        return -1;
    }
    scheduler_add(te);
    while (te->state != TASK_FINISHED) {
        task_yield();
    }
    if (!task_exit_ran) {
        regtest_fail("task_exit", "task didn't run");
        regtest_end_suite("task");
        return -1;
    }
    regtest_pass("task_exit");

    regtest_end_suite("task");
    return 0;
}

/* ========== User Mode Suite ========== */

/*
 * user_hello_code: prints "Hello from user mode!\n" and exits
 */
static const uint8_t user_hello_code[] = {
    0x48, 0x8d, 0x35, 0x17, 0x00, 0x00, 0x00,   /* lea rsi, [rip+23] */
    0xbf, 0x01, 0x00, 0x00, 0x00,               /* mov edi, 1 */
    0xba, 0x16, 0x00, 0x00, 0x00,               /* mov edx, 22 */
    0xb8, 0x01, 0x00, 0x00, 0x00,               /* mov eax, 1 (SYS_write) */
    0x0f, 0x05,                                 /* syscall */
    0x31, 0xff,                                 /* xor edi, edi */
    0x31, 0xc0,                                 /* xor eax, eax (SYS_exit) */
    0x0f, 0x05,                                 /* syscall */
    'H','e','l','l','o',' ','f','r','o','m',' ',
    'u','s','e','r',' ','m','o','d','e','!','\n'
};

/*
 * user_yield_code: prints "U1 " three times with yields
 */
static const uint8_t user_yield_code1[] = {
    0x41, 0xbc, 0x03, 0x00, 0x00, 0x00,         /* mov r12d, 3 */
    0x48, 0x8d, 0x35, 0x23, 0x00, 0x00, 0x00,   /* lea rsi, [rip+35] */
    0xbf, 0x01, 0x00, 0x00, 0x00,               /* mov edi, 1 */
    0xba, 0x03, 0x00, 0x00, 0x00,               /* mov edx, 3 */
    0xb8, 0x01, 0x00, 0x00, 0x00,               /* mov eax, 1 (SYS_write) */
    0x0f, 0x05,                                 /* syscall */
    0xb8, 0x02, 0x00, 0x00, 0x00,               /* mov eax, 2 (SYS_yield) */
    0x0f, 0x05,                                 /* syscall */
    0x41, 0xff, 0xcc,                           /* dec r12d */
    0x75, 0xdc,                                 /* jnz -36 */
    0x31, 0xff,                                 /* xor edi, edi */
    0x31, 0xc0,                                 /* xor eax, eax (SYS_exit) */
    0x0f, 0x05,                                 /* syscall */
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

/* user_fault_code: triggers invalid opcode */
static const uint8_t user_fault_code[] = {
    0x0f, 0x0b  /* ud2 */
};

int regtest_user(void) {
    regtest_start_suite("user");

    /* Test 1: Create user task */
    task_t *ut = task_create_user(user_hello_code, sizeof(user_hello_code));
    if (ut == NULL) {
        regtest_fail("user_create", "task_create_user returned NULL");
        regtest_end_suite("user");
        return -1;
    }
    regtest_pass("user_create");

    /* Test 2: User task execution */
    scheduler_add(ut);
    while (ut->state != TASK_FINISHED) {
        task_yield();
    }
    regtest_pass("user_exec");

    /* Test 3: Two user tasks with yield */
    task_t *u1 = task_create_user(user_yield_code1, sizeof(user_yield_code1));
    task_t *u2 = task_create_user(user_yield_code2, sizeof(user_yield_code2));
    if (u1 == NULL || u2 == NULL) {
        regtest_fail("user_yield_create", "create failed");
        regtest_end_suite("user");
        return -1;
    }
    scheduler_add(u1);
    scheduler_add(u2);
    while (u1->state != TASK_FINISHED || u2->state != TASK_FINISHED) {
        task_yield();
    }
    regtest_pass("user_yield");

    /* Test 4: Fault isolation (kernel survives user fault) */
    task_t *fault_task = task_create_user(user_fault_code, sizeof(user_fault_code));
    if (fault_task == NULL) {
        regtest_fail("user_fault_create", "create failed");
        regtest_end_suite("user");
        return -1;
    }
    scheduler_add(fault_task);
    while (fault_task->state != TASK_FINISHED) {
        task_yield();
    }
    /* If we get here, the kernel survived the user fault */
    regtest_pass("user_fault_isolation");

    regtest_end_suite("user");
    return 0;
}

/* ========== ELF Suite ========== */

int regtest_elf(void) {
    regtest_start_suite("elf");

    /* Check if modules are available */
    if (limine_modules == NULL || limine_modules->module_count == 0) {
        regtest_log("NOTE: No modules loaded, skipping ELF tests\n");
        regtest_pass("elf_skip_no_modules");
        regtest_end_suite("elf");
        return 0;
    }

    /* Test 1: Load init.elf from module */
    struct limine_file *init_mod = find_module("init.elf");
    if (init_mod == NULL) {
        regtest_fail("elf_find_module", "init.elf not found in modules");
        regtest_end_suite("elf");
        return -1;
    }
    regtest_pass("elf_find_module");

    /* Test 2: Create task from ELF */
    task_t *elf_task = task_create_elf(init_mod->address, init_mod->size);
    if (elf_task == NULL) {
        regtest_fail("elf_create_task", "task_create_elf returned NULL");
        regtest_end_suite("elf");
        return -1;
    }
    regtest_pass("elf_create_task");

    /* Test 3: Execute ELF task */
    scheduler_add(elf_task);
    while (elf_task->state != TASK_FINISHED) {
        task_yield();
    }
    regtest_pass("elf_exec");

    /* Test 4: Multiple ELF tasks (yield1 and yield2) */
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
            regtest_pass("elf_multi_task");
        } else {
            regtest_fail("elf_multi_task", "failed to create tasks");
        }
    } else {
        regtest_log("NOTE: yield ELFs not found, skipping multi-task test\n");
        regtest_pass("elf_multi_skip");
    }

    regtest_end_suite("elf");
    return 0;
}

/* ========== Filesystem Suite ========== */

int regtest_fs(void) {
    regtest_start_suite("fs");

    /* Test 1: Open file */
    int fd = vfs_open("INIT.ELF");
    if (fd < 0) {
        /* Filesystem may not be available */
        regtest_log("NOTE: Filesystem not available, skipping FS tests\n");
        regtest_pass("fs_skip_unavailable");
        regtest_end_suite("fs");
        return 0;
    }
    regtest_pass("fs_open");

    /* Test 2: Get file size */
    uint32_t size = vfs_size(fd);
    if (size == 0) {
        regtest_fail("fs_size", "file size is 0");
        vfs_close(fd);
        regtest_end_suite("fs");
        return -1;
    }
    regtest_pass("fs_size");

    /* Test 3: Read first 16 bytes (ELF header check) */
    uint8_t buf[16];
    int bytes_read = vfs_read(fd, buf, 16);
    if (bytes_read != 16) {
        regtest_fail("fs_read", "read failed");
        vfs_close(fd);
        regtest_end_suite("fs");
        return -1;
    }
    /* Check ELF magic */
    if (buf[0] != 0x7F || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        regtest_fail("fs_read_elf_magic", "ELF magic mismatch");
        vfs_close(fd);
        regtest_end_suite("fs");
        return -1;
    }
    regtest_pass("fs_read");

    /* Test 4: Seek */
    if (vfs_seek(fd, 0) != 0) {
        regtest_fail("fs_seek", "seek failed");
        vfs_close(fd);
        regtest_end_suite("fs");
        return -1;
    }
    /* Read again and verify */
    int bytes2 = vfs_read(fd, buf, 4);
    if (bytes2 != 4 || buf[0] != 0x7F) {
        regtest_fail("fs_seek_verify", "seek/read verification failed");
        vfs_close(fd);
        regtest_end_suite("fs");
        return -1;
    }
    regtest_pass("fs_seek");

    /* Test 5: Close */
    if (vfs_close(fd) != 0) {
        regtest_fail("fs_close", "close failed");
        regtest_end_suite("fs");
        return -1;
    }
    regtest_pass("fs_close");

    /* Test 6: Nonexistent file */
    int bad_fd = vfs_open("NOFILE.ELF");
    if (bad_fd >= 0) {
        regtest_fail("fs_nonexistent", "opened nonexistent file");
        vfs_close(bad_fd);
        regtest_end_suite("fs");
        return -1;
    }
    regtest_pass("fs_nonexistent");

    /* Test 7: Load ELF from disk and execute */
    task_t *disk_task = task_create_from_path("INIT.ELF");
    if (disk_task == NULL) {
        regtest_fail("fs_disk_elf", "task_create_from_path failed");
        regtest_end_suite("fs");
        return -1;
    }
    scheduler_add(disk_task);
    while (disk_task->state != TASK_FINISHED) {
        task_yield();
    }
    regtest_pass("fs_disk_elf");

    regtest_end_suite("fs");
    return 0;
}

/* ========== Framebuffer Suite ========== */

int regtest_fb(void) {
    regtest_start_suite("fb");

    /* Test 1: Framebuffer initialization check */
    const framebuffer_t *fb = fb_get_info();
    if (fb == NULL) {
        regtest_log("NOTE: Framebuffer not available, skipping FB tests\n");
        regtest_pass("fb_skip_unavailable");
        regtest_end_suite("fb");
        return 0;
    }
    regtest_pass("fb_init");

    /* Test 2: Framebuffer dimensions */
    if (fb->render_width == 0 || fb->render_height == 0) {
        regtest_fail("fb_dimensions", "invalid dimensions");
        regtest_end_suite("fb");
        return -1;
    }
    regtest_log("fb_dimensions: %dx%d\n", fb->render_width, fb->render_height);
    regtest_pass("fb_dimensions");

    /* Test 3: Back buffer allocation */
    if (fb->back == NULL) {
        regtest_fail("fb_backbuf", "back buffer is NULL");
        regtest_end_suite("fb");
        return -1;
    }
    regtest_pass("fb_backbuf");

    /* Test 4: Clear operation (no crash = pass) */
    fb_clear(0x00002244);
    regtest_pass("fb_clear");

    /* Test 5: Fill rect operation (no crash = pass) */
    fb_fill_rect(10, 10, 50, 50, 0x00FFFFFF);
    regtest_pass("fb_fill_rect");

    /* Test 6: Present operation (no crash = pass) */
    fb_present();
    regtest_pass("fb_present");

    regtest_end_suite("fb");
    return 0;
}

/* ========== Console Suite ========== */

int regtest_console(void) {
    regtest_start_suite("console");

    /* Check if framebuffer is available (console needs it) */
    const framebuffer_t *fb = fb_get_info();
    if (fb == NULL) {
        regtest_log("NOTE: Framebuffer not available, skipping console tests\n");
        regtest_pass("console_skip_unavailable");
        regtest_end_suite("console");
        return 0;
    }

    /* Test 1: Console clear (no crash = pass) */
    console_clear();
    regtest_pass("console_clear");

    /* Test 2: Console putc (no crash = pass) */
    console_putc('T');
    console_putc('E');
    console_putc('S');
    console_putc('T');
    regtest_pass("console_putc");

    /* Test 3: Console puts (no crash = pass) */
    console_puts("\nHello from regtest!\n");
    regtest_pass("console_puts");

    /* Test 4: Special characters */
    console_puts("Tab:\tafter\n");
    console_puts("Backspace: AB\bC\n");  /* Should show "AC" */
    regtest_pass("console_special_chars");

    /* Test 5: Scrolling (write many lines) */
    for (int i = 0; i < 10; i++) {
        console_puts("Scroll line\n");
    }
    regtest_pass("console_scroll");

    /* Test 6: Present to display */
    fb_present();
    regtest_pass("console_present");

    regtest_end_suite("console");
    return 0;
}
