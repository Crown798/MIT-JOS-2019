>Exercise 8. Change duppage in lib/fork.c to follow the new convention. If the page table entry has the PTE_SHARE bit set, just copy the mapping directly. (You should use PTE_SYSCALL, not 0xfff, to mask out the relevant bits from the page table entry. 0xfff picks up the accessed and dirty bits as well.)

Likewise, implement copy_shared_pages in lib/spawn.c. It should loop through all page table entries in the current process (just like fork did), copying any page mappings that have the PTE_SHARE bit set into the child process.

Use make run-testpteshare to check that your code is behaving properly. You should see lines that say "fork handles PTE_SHARE right" and "spawn handles PTE_SHARE right".

Use make run-testfdsharing to check that file descriptors are shared properly. You should see lines that say "read in child succeeded" and "read in parent succeeded".

# 代码

duppage 在 fork 中调用。
```
static int
duppage(envid_t envid, unsigned pn)
{
    int r;

    // LAB 4: Your code here.
    void *addr = (void*) (pn * PGSIZE);
    if (uvpt[pn] & PTE_SHARE) {
        sys_page_map(0, addr, envid, addr, PTE_SYSCALL);        //PTE_SHARE，两个进程都有读写权限
    } else if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) { //可写或者写时拷贝，需要同时标记当前进程和子进程的页表项为PTE_COW
        if ((r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
        if ((r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P)) < 0)
            panic("sys_page_map：%e", r);
    } else {
        sys_page_map(0, addr, envid, addr, PTE_U|PTE_P);    //只读，只需要拷贝映射关系
    }
    return 0;
}
```

copy_shared_pages 在 spawn 中调用。
```
static int
copy_shared_pages(envid_t child)
{
    // LAB 5: Your code here.
    uintptr_t addr;
    for (addr = 0; addr < UTOP; addr += PGSIZE) {
        if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) &&
                (uvpt[PGNUM(addr)] & PTE_U) && (uvpt[PGNUM(addr)] & PTE_SHARE)) {
            sys_page_map(0, (void*)addr, child, (void*)addr, (uvpt[PGNUM(addr)] & PTE_SYSCALL));
        }
    }
    return 0;
}
```
