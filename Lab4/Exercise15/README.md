>Exercise 15. Implement sys_ipc_recv and sys_ipc_try_send in kern/syscall.c. Read the comments on both before implementing them, since they have to work together. When you call envid2env in these routines, you should set the checkperm flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid.
```
Then implement the ipc_recv and ipc_send functions in lib/ipc.c.

Use the user/pingpong and user/primes functions to test your IPC mechanism. user/primes will generate for each prime number a new environment until JOS runs out of environments. You might find it interesting to read user/primes.c to see all the forking and IPC going on behind the scenes.
```

# 分析
页面共享的本质仍是修改页映射关系：修改 pte，并在 env 结构中记录相关信息。

# 代码

```
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
    // LAB 4: Your code here.
    struct Env *rcvenv;
    int ret = envid2env(envid, &rcvenv, 0);
    if (ret) return ret;
    if (!rcvenv->env_ipc_recving) return -E_IPC_NOT_RECV;

    if (srcva < (void*)UTOP) {
        pte_t *pte;
        struct PageInfo *pg = page_lookup(curenv->env_pgdir, srcva, &pte);

        //按照注释的顺序进行判定
        if (srcva != ROUNDDOWN(srcva, PGSIZE)) {        //srcva没有页对齐
            return -E_INVAL;
        }
        if ((*pte & perm & 7) != (perm & 7)) {  //perm应该是*pte的子集
            return -E_INVAL;
        }
        if (!pg) {          //srcva还没有映射到物理页
            return -E_INVAL;
        }
        if ((perm & PTE_W) && !(*pte & PTE_W)) {    //写权限
            return -E_INVAL;
        }       
        
        if (rcvenv->env_ipc_dstva < (void*)UTOP) {
            ret = page_insert(rcvenv->env_pgdir, pg, rcvenv->env_ipc_dstva, perm); //共享相同的映射关系
            if (ret) return ret;
            rcvenv->env_ipc_perm = perm;
        }
    }
    rcvenv->env_ipc_recving = 0;                    //标记接受进程可再次接受信息
    rcvenv->env_ipc_from = curenv->env_id;
    rcvenv->env_ipc_value = value; 
    rcvenv->env_status = ENV_RUNNABLE;
    rcvenv->env_tf.tf_regs.reg_eax = 0;
    return 0;
}
```

```
static int
sys_ipc_recv(void *dstva)
{
    // LAB 4: Your code here.
    if (dstva < (void *)UTOP && dstva != ROUNDDOWN(dstva, PGSIZE)) {
        return -E_INVAL;
    }
    curenv->env_ipc_recving = 1;
    curenv->env_status = ENV_NOT_RUNNABLE;
    curenv->env_ipc_dstva = dstva;
    sys_yield();
    return 0;
}
```

```
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
    // LAB 4: Your code here.
    if (pg == NULL) {
        pg = (void *)-1;
    }
    int r;
    while(1) {
        r = sys_ipc_try_send(to_env, val, pg, perm);
        if (r == 0) {       //发送成功
            return;
        } else if (r == -E_IPC_NOT_RECV) {  //接收进程没有准备好
            sys_yield();
        } else {            //其它错误
            panic("ipc_send():%e", r);
        }
    }
}
```

```
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
    // LAB 4: Your code here.
    if (pg == NULL) {
        pg = (void *)-1;
    }
    int r = sys_ipc_recv(pg);
    if (r < 0) {                //系统调用失败
        if (from_env_store) *from_env_store = 0;
        if (perm_store) *perm_store = 0;
        return r;
    }
    if (from_env_store)
        *from_env_store = thisenv->env_ipc_from;
    if (perm_store)
        *perm_store = thisenv->env_ipc_perm;
    return thisenv->env_ipc_value;
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
        case SYS_env_set_pgfault_upcall:
            ret = sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
            break;
        case SYS_ipc_try_send:
            ret = sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
            break;
        case SYS_ipc_recv:
            ret = sys_ipc_recv((void *)a1);
            break;
        default:
            return -E_INVAL;
	}
	return ret;
}
```
