/*
 * yield2.elf - User program that prints "Y2 " three times with yields, then exits
 */

.section .text
.global _start

_start:
    mov $3, %r12            /* loop counter */

.loop:
    /* write(1, message, 3) */
    mov $1, %rax            /* SYS_write */
    mov $1, %rdi            /* fd = stdout */
    lea message(%rip), %rsi /* buf = message */
    mov $3, %rdx            /* len */
    syscall

    /* yield() */
    mov $2, %rax            /* SYS_yield */
    syscall

    dec %r12
    jnz .loop

    /* exit(0) */
    mov $0, %rax            /* SYS_exit */
    xor %rdi, %rdi          /* code = 0 */
    syscall

    ud2

.section .rodata
message:
    .ascii "Y2 "
