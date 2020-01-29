>Exercise 7. Implement the system calls described above in kern/syscall.c and make sure syscall() calls them. You will need to use various functions in kern/pmap.c and kern/env.c, particularly envid2env(). For now, whenever you call envid2env(), pass 1 in the checkperm parameter. Be sure you check for any invalid system call arguments, returning -E_INVAL in that case. Test your JOS kernel with user/dumbfork and make sure it works before proceeding.

# 代码

```
static envid_t
sys_exofork(void)
{
    // Create the new environment with env_alloc(), from kern/env.c.
    // It should be left as env_alloc created it, except that
    // status is set to ENV_NOT_RUNNABLE, and the register set is copied
    // from the current environment -- but tweaked so sys_exofork
    // will appear to return 0.

    // LAB 4: Your code here.
    struct Env *e;
    int ret = env_alloc(&e, curenv->env_id);    //分配一个Env结构
    if (ret < 0) {
        return ret;
    }
    e->env_tf = curenv->env_tf;         //寄存器状态和当前进程一致
    e->env_status = ENV_NOT_RUNNABLE;   //目前还不能运行
    e->env_tf.tf_regs.reg_eax = 0;      //新的进程从sys_exofork()的返回值应该为0
    return e->env_id;
}
```

```
static int
sys_env_set_status(envid_t envid, int status)
{
    // Hint: Use the 'envid2env' function from kern/env.c to translate an
    // envid to a struct Env.
    // You should set envid2env's third argument to 1, which will
    // check whether the current environment has permission to set
    // envid's status.
    if (status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE) return -E_INVAL;

    struct Env *e;
    int ret = envid2env(envid, &e, 1);
    if (ret < 0) {
        return ret;
    }
    e->env_status = status;
    return 0;
}
```

```
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
    // Hint: This function is a wrapper around page_alloc() and
    //   page_insert() from kern/pmap.c.
    //   Most of the new code you write should be to check the
    //   parameters for correctness.
    //   If page_insert() fails, remember to free the page you
    //   allocated!

    // LAB 4: Your code here.
    struct Env *e;                                  
    int ret = envid2env(envid, &e, 1);
    if (ret) return ret;    //bad_env

    if ((va >= (void*)UTOP) || (ROUNDDOWN(va, PGSIZE) != va)) return -E_INVAL;      
    int flag = PTE_U | PTE_P;
    if ((perm & flag) != flag) return -E_INVAL;

    struct PageInfo *pg = page_alloc(1);            //分配物理页
    if (!pg) return -E_NO_MEM;

    ret = page_insert(e->env_pgdir, pg, va, perm);  //建立映射关系
    if (ret) {
        page_free(pg);
        return ret;
    }

    return 0;
}
```

```
static int
sys_page_map(envid_t srcenvid, void *srcva,
         envid_t dstenvid, void *dstva, int perm)
{
    // Hint: This function is a wrapper around page_lookup() and
    //   page_insert() from kern/pmap.c.
    //   Again, most of the new code you write should be to check the
    //   parameters for correctness.
    //   Use the third argument to page_lookup() to
    //   check the current permissions on the page.

    // LAB 4: Your code here.
    struct Env *se, *de;
    int ret = envid2env(srcenvid, &se, 1);
    if (ret) return ret;    //bad_env
    ret = envid2env(dstenvid, &de, 1);
    if (ret) return ret;    //bad_env

    //  -E_INVAL if srcva >= UTOP or srcva is not page-aligned,
    //      or dstva >= UTOP or dstva is not page-aligned.
    if (srcva >= (void*)UTOP || dstva >= (void*)UTOP || 
        ROUNDDOWN(srcva,PGSIZE) != srcva || ROUNDDOWN(dstva,PGSIZE) != dstva) 
        return -E_INVAL;

    //  -E_INVAL if srcva is not mapped in srcenvid's address space.
    pte_t *pte;
    struct PageInfo *pg = page_lookup(se->env_pgdir, srcva, &pte);
    if (!pg) return -E_INVAL;

    //  -E_INVAL if perm is inappropriate (see sys_page_alloc).
    int flag = PTE_U|PTE_P;
    if ((perm & flag) != flag) return -E_INVAL;

    //  -E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
    //      address space.
    if ((!(*pte & PTE_W)) && (perm & PTE_W)) return -E_INVAL;

    //  -E_NO_MEM if there's no memory to allocate any necessary page tables.
    ret = page_insert(de->env_pgdir, pg, dstva, perm);
    return ret;
}
```

```
static int
sys_page_unmap(envid_t envid, void *va)
{
    // Hint: This function is a wrapper around page_remove().

    // LAB 4: Your code here.
    struct Env *env;
    int ret = envid2env(envid, &env, 1);
    if (ret) return ret;

    if ((va >= (void*)UTOP) || (ROUNDDOWN(va, PGSIZE) != va)) return -E_INVAL;
	
    page_remove(env->env_pgdir, va);
    return 0;
}
```

```
// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	//panic("syscall not implemented");
	int32_t ret;
	switch (syscallno) {
		case SYS_cputs:
        sys_cputs((char *)a1, (size_t)a2);
        ret = 0;
        break;
    case SYS_cgetc:
        ret = sys_cgetc();
        break;
    case SYS_getenvid:
        ret = sys_getenvid();
        break;
    case SYS_env_destroy:
        ret = sys_env_destroy((envid_t)a1);
        break;
    case SYS_yield:
        sys_yield();
        ret = 0;
        break;
    case SYS_page_alloc:
        ret = sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
        break;
    case SYS_page_map:
        ret = sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
        break;
    case SYS_page_unmap:
        ret = sys_page_unmap((envid_t)a1, (void *)a2);
        break;
    case SYS_exofork:
        ret = sys_exofork();
        break;
    case SYS_env_set_status:
        ret = sys_env_set_status((envid_t)a1, (int)a2);
        break;
    default:
        return -E_INVAL;
	}
	return ret;
}
```