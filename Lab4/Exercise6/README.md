>Exercise 6. Implement round-robin scheduling in sched_yield() as described above. Don't forget to modify syscall() to dispatch sys_yield().
```
Make sure to invoke sched_yield() in mp_main.

Modify kern/init.c to create three (or more!) environments that all run the program user/yield.c.

Run make qemu. You should see the environments switch back and forth between each other five times before terminating, like below.

Test also with several CPUS: make qemu CPUS=2.
...
Hello, I am environment 00001000.
Hello, I am environment 00001001.
Hello, I am environment 00001002.
Back in environment 00001000, iteration 0.
Back in environment 00001001, iteration 0.
Back in environment 00001002, iteration 0.
Back in environment 00001000, iteration 1.
Back in environment 00001001, iteration 1.
Back in environment 00001002, iteration 1.
...
After the yield programs exit, there will be no runnable environment in the system, the scheduler should invoke the JOS kernel monitor. If any of this does not happen, then fix your code before proceeding.
```

# 代码

```
void
sched_yield(void)
{
    struct Env *idle;

    // Implement simple round-robin scheduling.
    //
    // Search through 'envs' for an ENV_RUNNABLE environment in
    // circular fashion starting just after the env this CPU was
    // last running.  Switch to the first such environment found.
    //
    // If no envs are runnable, but the environment previously
    // running on this CPU is still ENV_RUNNING, it's okay to
    // choose that environment. Make sure curenv is not null before
    // dereferencing it.
    //
    // Never choose an environment that's currently running on
    // another CPU (env_status == ENV_RUNNING). If there are
    // no runnable environments, simply drop through to the code
    // below to halt the cpu.

    // LAB 4: Your code here.
    int start = 0;
    int j;
    if (curenv) {
        start = ENVX(curenv->env_id) + 1;   //从当前Env结构的后一个开始
    }
    for (int i = 0; i < NENV; i++) {        //遍历所有Env结构
        j = (start + i) % NENV;
        if (envs[j].env_status == ENV_RUNNABLE) {
            env_run(&envs[j]);              //env_run不会返回，使用保存在 env 结构中的 tf 信息（非指针）
        }
    }
    if (curenv && curenv->env_status == ENV_RUNNING) {      //重新执行之前运行的进程
        env_run(curenv);
    }

    // sched_halt never returns
    sched_halt();
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
    default:
        return -E_INVAL;
	}
	return ret;
}
```
