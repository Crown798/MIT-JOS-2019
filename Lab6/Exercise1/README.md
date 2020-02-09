>Exercise 1. Add a call to time_tick for every clock interrupt in kern/trap.c. Implement sys_time_msec and add it to syscall in kern/syscall.c so that user space has access to the time.

# 代码

在kern/trap.c中添加对time_tick()调用
```
// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.
	// Add time tick increment to clock interrupts.
	// Be careful! In multiprocessors, clock interrupts are
	// triggered on every CPU.
	// LAB 6: Your code here.
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		time_tick();
        sched_yield();
        return;
	}

```

实现sys_time_msec()系统调用
```
// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	// panic("sys_time_msec not implemented");
	return time_msec();
}

case sys_time_msec:
        ret = sys_time_msec();
        break;
```