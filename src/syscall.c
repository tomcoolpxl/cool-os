#include "syscall.h"
#include "msr.h"
#include "gdt.h"
#include "serial.h"
#include "task.h"
#include "scheduler.h"

/* Assembly entry point */
extern void syscall_entry(void);

void syscall_init(void) {
    uint64_t efer;

    /* Enable SYSCALL/SYSRET and NX in EFER */
    efer = rdmsr(MSR_IA32_EFER);
    efer |= EFER_SCE | EFER_NXE;
    wrmsr(MSR_IA32_EFER, efer);

    /*
     * STAR MSR layout (SYSCALL target):
     *   Bits 63:48 = User CS base for SYSRET (SS = base+8, CS = base+16)
     *   Bits 47:32 = Kernel CS for SYSCALL (SS = CS+8)
     *   Bits 31:0  = Reserved (32-bit SYSCALL target, unused in long mode)
     *
     * For SYSRET with User Data @ 0x18 and User Code @ 0x20:
     *   base = 0x10 (so SS = 0x18, CS = 0x20, plus RPL=3 gives 0x1B/0x23)
     * For SYSCALL with Kernel Code @ 0x08:
     *   kernel CS = 0x08 (SS = 0x10)
     */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)KERNEL_CS << 32);
    wrmsr(MSR_IA32_STAR, star);

    /* LSTAR = 64-bit SYSCALL entry point */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    /*
     * FMASK = flags to clear on SYSCALL
     * Clear IF (0x200) to disable interrupts in kernel
     * Clear DF (0x400) for ABI compliance
     * Clear TF (0x100) to disable single-stepping
     */
    wrmsr(MSR_IA32_FMASK, 0x700);

    serial_puts("SYSCALL: Initialized MSRs\n");
}

/* Syscall: exit(code) - terminate the current task */
static void sys_exit(uint64_t code) {
    task_exit((int)code);
    /* Should not return */
}

/* Syscall: write(fd, buf, len) - write to fd (only fd=1 supported) */
static uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    if (fd != 1) {
        return (uint64_t)-1;  /* Only stdout supported */
    }

    const char *s = (const char *)buf;
    for (uint64_t i = 0; i < len; i++) {
        serial_putc(s[i]);
    }
    return len;
}

/* Syscall: yield() - voluntarily give up CPU */
static void sys_yield(void) {
    scheduler_yield();
}

/* Syscall: wait(status) - wait for child to exit */
static int64_t sys_wait(uint64_t status_ptr) {
    int status;
    int pid = task_wait(&status);

    /* Copy status to user space if pointer valid */
    if (pid > 0 && status_ptr != 0) {
        /* Validate user pointer is in user address range */
        if (status_ptr < 0x800000000000ULL) {  /* Low canonical half */
            *(int *)status_ptr = status;
        }
    }

    return pid;
}

/* Syscall: getpid() - get current process ID */
static uint64_t sys_getpid(void) {
    return task_getpid();
}

/* Syscall: getppid() - get parent process ID */
static uint64_t sys_getppid(void) {
    return task_getppid();
}

uint64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (num) {
        case SYS_exit:
            sys_exit(arg1);
            return 0;  /* Never reached */

        case SYS_write:
            return sys_write(arg1, arg2, arg3);

        case SYS_yield:
            sys_yield();
            return 0;

        case SYS_wait:
            return (uint64_t)sys_wait(arg1);

        case SYS_getpid:
            return sys_getpid();

        case SYS_getppid:
            return sys_getppid();

        default:
            /* Unknown syscall */
            return (uint64_t)-1;
    }
}
