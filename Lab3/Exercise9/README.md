>Exercise 9. Change kern/trap.c to panic if a page fault happens in kernel mode.
```
Hint: to determine whether a fault happened in user mode or in kernel mode, check the low bits of the tf_cs.

Read user_mem_assert in kern/pmap.c and implement user_mem_check in that same file.

Change kern/syscall.c to sanity check arguments to system calls.

Boot your kernel, running user/buggyhello. The environment should be destroyed, and the kernel should not panic. You should see:

	[00001000] user_mem_check assertion failure for va 00000001
	[00001000] free env 00001000
	Destroyed the only environment - nothing more to do!
	
Finally, change debuginfo_eip in kern/kdebug.c to call user_mem_check on usd, stabs, and stabstr. 
If you now run user/breakpoint, you should be able to run backtrace from the kernel monitor and see the backtrace traverse into lib/libmain.c before the kernel panics with a page fault. 
What causes this page fault? You don't need to fix it, but you should understand why it happens.
```

# 代码

在page_fault_handler()中添加如下代码：
```
void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if((tf->tf_cs & 3) == 0) {
		panic("page_fault_handler():page fault in kernel mode!\n");
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
```

实现kern/pmap.c中的user_mem_check()权限检查函数：
```
// Check that an environment is allowed to access the range of memory
// [va, va+len) with permissions 'perm | PTE_P'.
// Normally 'perm' will contain PTE_U at least, but this is not required.
// 'va' and 'len' need not be page-aligned; you must test every page that
// contains any of that range.  You will test either 'len/PGSIZE',
// 'len/PGSIZE + 1', or 'len/PGSIZE + 2' pages.
//
// A user program can access a virtual address if (1) the address is below
// ULIM, and (2) the page table gives it permission.  These are exactly
// the tests you should implement here.
//
// If there is an error, set the 'user_mem_check_addr' variable to the first
// erroneous virtual address.
//
// Returns 0 if the user program can access this range of addresses,
// and -E_FAULT otherwise.
//
int
user_mem_check(struct Env *env, const void *va, size_t len, int perm)
{
	// LAB 3: Your code here.
	uintptr_t p = (uintptr_t) va;
	uintptr_t end = (uintptr_t)(p + len);
	p = (uintptr_t)ROUNDDOWN(p, PGSIZE);
	end = (uintptr_t)ROUNDUP(end, PGSIZE);
	for(;p < end; p += PGSIZE) {
		pte_t *pte = pgdir_walk(env->env_pgdir, (void *)p, 0);
		if((p >= ULIM) || pte == NULL || !(*pte & PTE_P) || ((*pte & perm) != perm)) {
			user_mem_check_addr = (p < (uint32_t)va ? (uint32_t)va : p);
			return -E_FAULT;
		}
	}
	return 0;
}

//
// Checks that environment 'env' is allowed to access the range
// of memory [va, va+len) with permissions 'perm | PTE_U | PTE_P'.
// If it can, then the function simply returns.
// If it cannot, 'env' is destroyed and, if env is the current
// environment, this function will not return.
//
void
user_mem_assert(struct Env *env, const void *va, size_t len, int perm)
{
	if (user_mem_check(env, va, len, perm | PTE_U) < 0) {
		cprintf("[%08x] user_mem_check assertion failure for "
			"va %08x\n", env->env_id, user_mem_check_addr);
		env_destroy(env);	// may not return
	}
}
```

kern/syscall.c中的系统调用函数sys_cputs()的参数中有指针，所以需要对其进行检测：
```
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, (void *)s, len, PTE_U | PTE_P);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}
```

在 kern/kdebug.c 的 debuginfo_eip 中，使用 user_mem_check 对 usd, stabs, and stabstr 进行检查：
```
const struct UserStabData *usd = (const struct UserStabData *) USTABDATA;

		// Make sure this memory is valid.
		// Return -1 if it is not.  Hint: Call user_mem_check.
		// LAB 3: Your code here.
		int r;
		if((r = user_mem_check(curenv, (void *)USTABDATA, sizeof(struct UserStabData), PTE_P|PTE_U)) < 0)
			return -1;
```

当 breakpoint 陷入 monitor 之后，如果执行 backtrace，则最终会发生 page fault。

这是因为 backtrace 借助 ebp 在栈中游走时，会从内核栈跨越到用户栈，最终走出用户栈访问到了无效的内存地址：
```
K> backtrace
ebp efffff20 eip f01008d5 args 00000001 efffff38 f01b4000 00000000 f0172840
next ebp is at efffff90
kern/monitor.c:137: monitor+-267384895
ebp efffff90 eip f010354a args f01b4000 efffffbc 00000000 00000082 00000000
next ebp is at efffffb0
kern/trap.c:185: trap+-267373400
ebp efffffb0 eip f0103647 args efffffbc 00000000 00000000 eebfdfd0 efffffdc
next ebp is at eebfdfd0
kern/syscall.c:69: syscall+-267372985
ebp eebfdfd0 eip 00800073 args 00000000 00000000 eebfdff0 00800049 00000000
next ebp is at eebfdff0
lib/libmain.c:27: libmain+8388665
Incoming TRAP frame at 0xeffffebc
kernel panic at kern/trap.c:256: page_fault_handler():page fault in kernel mode!
```
由 memlayout.h 中的用户地址空间布局可知，eebfdff0 地址处于无效区域，所以会产生页错误。