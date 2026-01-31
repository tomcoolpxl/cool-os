/*
 * crt0.S - C Runtime Entry Point
 *
 * This is the entry point for C user programs.
 * It sets up the environment and calls main().
 */

.section .text
.global _start

_start:
    /* Clear base pointer for clean stack traces */
    xor %rbp, %rbp

    /* Align stack to 16 bytes (System V ABI requirement) */
    and $-16, %rsp

    /* Call main(argc=0, argv=NULL) */
    xor %edi, %edi          /* argc = 0 */
    xor %esi, %esi          /* argv = NULL */
    call main

    /* exit(return_value) */
    mov %eax, %edi
    call exit

    /* Should not reach here, but halt if we do */
1:
    hlt
    jmp 1b
