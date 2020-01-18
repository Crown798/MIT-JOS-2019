>Exercise 6. Modify trap_dispatch() to make breakpoint exceptions invoke the kernel monitor. You should now be able to get make grade to succeed on the breakpoint test.

# 代码

注意在 trap_init 设置 IDT 时，对应 breakpoint exception 的表项的 DPL 应设置为 3 使得用户态下也能够进入内核（伪系统调用），否则会触发 13 号中断 general protection fault。
```
void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
  void th0();
  void th1();
  void th3();
  void th4();
  void th5();
  void th6();
  void th7();
  void th8();
  void th9();
  void th10();
  void th11();
  void th12();
  void th13();
  void th14();
  void th16();
  void th_syscall();
  SETGATE(idt[0], 0, GD_KT, th0, 0);    //格式如下：SETGATE(gate, istrap, sel, off, dpl)，定义在inc/mmu.h中
  SETGATE(idt[1], 0, GD_KT, th1, 0);
  SETGATE(idt[3], 0, GD_KT, th3, 3);
  SETGATE(idt[4], 0, GD_KT, th4, 0);
  SETGATE(idt[5], 0, GD_KT, th5, 0);
  SETGATE(idt[6], 0, GD_KT, th6, 0);
  SETGATE(idt[7], 0, GD_KT, th7, 0);
  SETGATE(idt[8], 0, GD_KT, th8, 0);
  SETGATE(idt[9], 0, GD_KT, th9, 0);
  SETGATE(idt[10], 0, GD_KT, th10, 0);
  SETGATE(idt[11], 0, GD_KT, th11, 0);
  SETGATE(idt[12], 0, GD_KT, th12, 0);
  SETGATE(idt[13], 0, GD_KT, th13, 0);
  SETGATE(idt[14], 0, GD_KT, th14, 0);
  SETGATE(idt[16], 0, GD_KT, th16, 0);

  SETGATE(idt[T_SYSCALL], 1, GD_KT, th_syscall, 3);

	// Per-CPU setup
	trap_init_percpu();
}
```

```
static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	if(tf->tf_trapno == T_PGFLT) {
		page_fault_handler(tf);
		return;
	}
	else if(tf->tf_trapno == T_BRKPT) {
		monitor(tf);
		return;
	}

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}
```
