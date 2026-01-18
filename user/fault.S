/*
 * fault.elf - User program that attempts to access kernel memory (should fault)
 *
 * This tests privilege separation by trying to jump to a kernel address.
 * The CPU should generate a page fault (user trying to access supervisor page).
 */

.section .text
.global _start

_start:
    /* Try to jump to a kernel address - this should cause a page fault */
    movabs $0xffffffff80000000, %rax
    jmp *%rax

    /* Should never reach here */
    ud2
