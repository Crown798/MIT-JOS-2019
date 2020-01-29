>Exercise 9. Implement the code in page_fault_handler in kern/trap.c required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)

# 代码

```
void
page_fault_handler(struct Trapframe *tf)
{
    uint32_t fault_va;

    // Read processor's CR2 register to find the faulting address
    fault_va = rcr2();

    // Handle kernel-mode page faults.
    // LAB 3: Your code here.
    if ((tf->tf_cs & 3) == 0)
        panic("page_fault_handler():page fault in kernel mode!\n");

    // LAB 4: Your code here.
    if (curenv->env_pgfault_upcall) {
        uintptr_t stacktop = UXSTACKTOP;
        if (UXSTACKTOP - PGSIZE < tf->tf_esp && tf->tf_esp < UXSTACKTOP) {
            stacktop = tf->tf_esp;
        }
        uint32_t size = sizeof(struct UTrapframe) + sizeof(uint32_t);
        user_mem_assert(curenv, (void *)stacktop - size, size, PTE_U | PTE_W);
        struct UTrapframe *utr = (struct UTrapframe *)(stacktop - size);
        utr->utf_fault_va = fault_va;
        utr->utf_err = tf->tf_err;
        utr->utf_regs = tf->tf_regs;
        utr->utf_eip = tf->tf_eip;
        utr->utf_eflags = tf->tf_eflags;
        utr->utf_esp = tf->tf_esp;              

        curenv->env_tf.tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
        curenv->env_tf.tf_esp = (uintptr_t)utr;
        env_run(curenv);            //进入用户态页异常处理函数，不会返回
    }

    // Destroy the environment that caused the fault.
    cprintf("[%08x] user fault va %08x ip %08x\n",
        curenv->env_id, fault_va, tf->tf_eip);
    print_trapframe(tf);
    env_destroy(curenv);
}
```