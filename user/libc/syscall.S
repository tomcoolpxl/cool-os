/*
 * syscall.S - Low-level syscall wrapper
 *
 * x86-64 syscall ABI:
 *   RAX = syscall number
 *   RDI = arg1, RSI = arg2, RDX = arg3, R10 = arg4, R8 = arg5, R9 = arg6
 *   Return value in RAX
 *
 * Note: The syscall instruction clobbers RCX and R11.
 */

.section .text

/*
 * long _syscall3(long num, long arg1, long arg2, long arg3)
 *
 * Arguments arrive in: RDI=num, RSI=arg1, RDX=arg2, RCX=arg3
 * Syscall wants:       RAX=num, RDI=arg1, RSI=arg2, RDX=arg3
 */
.global _syscall3
_syscall3:
    mov %rdi, %rax      /* num -> RAX */
    mov %rsi, %rdi      /* arg1 -> RDI */
    mov %rdx, %rsi      /* arg2 -> RSI */
    mov %rcx, %rdx      /* arg3 -> RDX */
    syscall
    ret

/*
 * long _syscall1(long num, long arg1)
 */
.global _syscall1
_syscall1:
    mov %rdi, %rax      /* num -> RAX */
    mov %rsi, %rdi      /* arg1 -> RDI */
    syscall
    ret

/*
 * long _syscall0(long num)
 */
.global _syscall0
_syscall0:
    mov %rdi, %rax      /* num -> RAX */
    syscall
    ret
