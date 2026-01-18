/*
 * init.elf - Simple user program that prints "Hello from ELF user mode!" and exits
 *
 * Syscall ABI:
 *   RAX = syscall number (0=exit, 1=write, 2=yield)
 *   RDI = arg1, RSI = arg2, RDX = arg3
 *   SYSCALL instruction
 */

.section .text
.global _start

_start:
    /* write(1, message, 25) */
    mov $1, %rax            /* SYS_write */
    mov $1, %rdi            /* fd = stdout */
    lea message(%rip), %rsi /* buf = message */
    mov $25, %rdx           /* len */
    syscall

    /* exit(0) */
    mov $0, %rax            /* SYS_exit */
    xor %rdi, %rdi          /* code = 0 */
    syscall

    /* Should never reach here */
    ud2

.section .rodata
message:
    .ascii "Hello from ELF user mode!\n"
