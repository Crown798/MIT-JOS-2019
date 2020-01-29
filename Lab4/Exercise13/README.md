>Exercise 13. Modify kern/trapentry.S and kern/trap.c to initialize the appropriate entries in the IDT and provide handlers for IRQs 0 through 15. Then modify the code in env_alloc() in kern/env.c to ensure that user environments are always run with interrupts enabled.
```
Also uncomment the sti instruction in sched_halt() so that idle CPUs unmask interrupts.

The processor never pushes an error code when invoking a hardware interrupt handler. You might want to re-read section 9.2 of the 80386 Reference Manual, or section 5.8 of the IA-32 Intel Architecture Software Developer's Manual, Volume 3, at this time.

After doing this exercise, if you run your kernel with any test program that runs for a non-trivial length of time (e.g., spin), you should see the kernel print trap frames for hardware interrupts. While interrupts are now enabled in the processor, JOS isn't yet handling them, so you should see it misattribute each interrupt to the currently running user environment and destroy it. Eventually it should run out of environments to destroy and drop into the monitor.
```

# 代码

```
/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
  TRAPHANDLER_NOEC(th0, 0)
  TRAPHANDLER_NOEC(th1, 1)
  TRAPHANDLER_NOEC(th3, 3)
  TRAPHANDLER_NOEC(th4, 4)
  TRAPHANDLER_NOEC(th5, 5)
  TRAPHANDLER_NOEC(th6, 6)
  TRAPHANDLER_NOEC(th7, 7)
  TRAPHANDLER(th8, 8)
  TRAPHANDLER_NOEC(th9, 9)
  TRAPHANDLER(th10, 10)
  TRAPHANDLER(th11, 11)
  TRAPHANDLER(th12, 12)
  TRAPHANDLER(th13, 13)
  TRAPHANDLER(th14, 14)
  TRAPHANDLER_NOEC(th16, 16)

  TRAPHANDLER_NOEC(th_syscall, T_SYSCALL)

  TRAPHANDLER_NOEC(timer_handler, IRQ_OFFSET + IRQ_TIMER);
  TRAPHANDLER_NOEC(kbd_handler, IRQ_OFFSET + IRQ_KBD);
  TRAPHANDLER_NOEC(serial_handler, IRQ_OFFSET + IRQ_SERIAL);
  TRAPHANDLER_NOEC(spurious_handler, IRQ_OFFSET + IRQ_SPURIOUS);
  TRAPHANDLER_NOEC(ide_handler, IRQ_OFFSET + IRQ_IDE);
  TRAPHANDLER_NOEC(error_handler, IRQ_OFFSET + IRQ_ERROR);

/*
 * Lab 3: Your code here for _alltraps
 */
   _alltraps:
   pushl %ds
   pushl %es
   pushal

   movw	   $GD_KD, %ax
   movw    %ax, %ds
   movw    %ax, %es

   pushl %esp  //压入trap()的参数tf，%esp指向Trapframe结构的起始地址
   call trap       //调用trap()函数
```

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
    void timer_handler();
    void kbd_handler();
    void serial_handler();
    void spurious_handler();
    void ide_handler();
    void error_handler();

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

    SETGATE(idt[T_SYSCALL], 0, GD_KT, th_syscall, 3);

	SETGATE(idt[IRQ_OFFSET + IRQ_TIMER],    0, GD_KT, timer_handler, 0);
	SETGATE(idt[IRQ_OFFSET + IRQ_KBD],      0, GD_KT, kbd_handler,     0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL],   0, GD_KT, serial_handler,  0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SPURIOUS], 0, GD_KT, spurious_handler, 0);
	SETGATE(idt[IRQ_OFFSET + IRQ_IDE],      0, GD_KT, ide_handler,     0);
	SETGATE(idt[IRQ_OFFSET + IRQ_ERROR],    0, GD_KT, error_handler,   0);

	// Per-CPU setup
	trap_init_percpu();
}
```

```
int
env_alloc(struct Env **newenv_store, envid_t parent_id)
{

	// Enable interrupts while in user mode.
	// LAB 4: Your code here.
	e->env_tf.tf_eflags |= FL_IF;

}
```

```
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}
```