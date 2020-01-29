>Exercise 11. Finish set_pgfault_handler() in lib/pgfault.c.

# 代码

这里需要同时分配用户异常栈、设置用户页异常处理的汇编部分 env_pgfault_upcall。
```
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
    int r;

    if (_pgfault_handler == 0) {
        // First time through!
        // LAB 4: Your code here.
        int r = sys_page_alloc(0, (void *)(UXSTACKTOP-PGSIZE), PTE_W | PTE_U | PTE_P);  //分配用户异常栈
        if (r < 0) {
            panic("set_pgfault_handler:sys_page_alloc failed");;
        }
        
        sys_env_set_pgfault_upcall(0, _pgfault_upcall);     //设置进程的env_pgfault_upcall
    }

    // Save handler pointer for assembly to call.
    _pgfault_handler = handler;
}
```