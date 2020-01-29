>Exercise 12. Implement fork, duppage and pgfault in lib/fork.c.
```
Test your code with the forktree program. It should produce the following messages, with interspersed 'new env', 'free env', and 'exiting gracefully' messages. The messages may not appear in this order, and the environment IDs may be different.

	1000: I am ''
	1001: I am '0'
	2000: I am '00'
	2001: I am '000'
	1002: I am '1'
	3000: I am '11'
	3001: I am '10'
	4000: I am '100'
	1003: I am '01'
	5000: I am '010'
	4001: I am '011'
	2002: I am '110'
	1004: I am '001'
	1005: I am '111'
	1006: I am '101'
```

# 代码

```
static int
duppage(envid_t envid, unsigned pn)
{
    int r;

    // LAB 4: Your code here.
    void *addr = (void*) (pn * PGSIZE);
    if (uvpt[pn] & PTE_SHARE) {
        sys_page_map(0, addr, envid, addr, PTE_SYSCALL);        //对于PTE_SHARE的页，两个进程都有读写权限
    } 
    else if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {      //对于可写的或者写时拷贝的页，同时标记当两个进程的页表项为PTE_COW
        if ((r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
        if ((r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
    } 
    else {
        sys_page_map(0, addr, envid, addr, PTE_U|PTE_P);    //对于只读的页
    }
    return 0;
}
```

```
envid_t
fork(void)
{
    // LAB 4: Your code here.
    set_pgfault_handler(pgfault);   

    envid_t envid = sys_exofork();  
    if (envid == 0) {               //子进程分支
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }
    if (envid < 0) {
        panic("sys_exofork: %e", envid);
    }

    uint32_t addr;
    for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
        if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)) {
            duppage(envid, PGNUM(addr));    //拷贝进程映射关系
        }
    }

    int r;
    if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_P | PTE_W | PTE_U)) < 0)    //分配异常栈
        panic("sys_page_alloc: %e", r);

    extern void _pgfault_upcall(void);
    sys_env_set_pgfault_upcall(envid, _pgfault_upcall);     //为子进程设置_pgfault_upcall

    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)  //设置子进程为ENV_RUNNABLE状态
        panic("sys_env_set_status: %e", r);

    return envid;
}
```

```
static void
pgfault(struct UTrapframe *utf)
{

    // Check that the faulting access was (1) a write, and (2) to a
    // copy-on-write page.  If not, panic.
    // Hint:
    //   Use the read-only page table mappings at uvpt
    //   (see <inc/memlayout.h>).
    // LAB 4: Your code here.
    uint32_t err = utf->utf_err;
    if (!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW))) { 
        panic("pgfault():not cow");
    }

    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.
    // LAB 4: Your code here.
    void *addr = (void *) utf->utf_fault_va;
    addr = ROUNDDOWN(addr, PGSIZE);
    int r;
    if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)   //给PFTEMP分配映射物理页
        panic("sys_page_alloc: %e", r);
    memmove(PFTEMP, addr, PGSIZE);                              //将addr指向的物理页拷贝到PFTEMP指向的物理页
    if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_U|PTE_P|PTE_W)) < 0)        //将当前进程addr也映射到当前进程PFTEMP指向的物理页
        panic("sys_page_map: %e", r);
    if ((r = sys_page_unmap(0, PFTEMP)) < 0)                    //解除当前进程PFTEMP映射
        panic("sys_page_unmap: %e", r);
}
```
