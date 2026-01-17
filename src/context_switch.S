.code64
.global context_switch

/* void context_switch(task_t *old, task_t *new)
 * RDI = old task, RSI = new task
 * Saves callee-saved regs to old stack, switches to new stack, restores regs
 */
context_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)       /* old->rsp = RSP */
    movq (%rsi), %rsp       /* RSP = new->rsp */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
