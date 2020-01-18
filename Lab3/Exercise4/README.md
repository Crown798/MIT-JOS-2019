>Exercise 4. Edit trapentry.S and trap.c and implement the features described above. The macros TRAPHANDLER and TRAPHANDLER_NOEC in trapentry.S should help you, as well as the T_* defines in inc/trap.h. You will need to add an entry point in trapentry.S (using those macros) for each trap defined in inc/trap.h, and you'll have to provide _alltraps which the TRAPHANDLER macros refer to. You will also need to modify trap_init() to initialize the idt to point to each of these entry points defined in trapentry.S; the SETGATE macro will be helpful here.
> Your _alltraps should:
```
push values to make the stack look like a struct Trapframe
load GD_KD into %ds and %es
pushl %esp to pass a pointer to the Trapframe as an argument to trap()
call trap (can trap ever return?)

Consider using the pushal instruction; it fits nicely with the layout of the struct Trapframe.
```
> Test your trap handling code using some of the test programs in the user directory that cause exceptions before making any system calls, such as user/divzero. You should be able to get make grade to succeed on the divzero, softint, and badsegment tests at this point.

# 分析
进入 trap() 之前所完成的 trapframe 结构如下：
```
struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;    // 中断号
	/* below here defined by x86 hardware */
	uint32_t tf_err;       // 错误码，有的真正有，有的仅为0以保持结构统一
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	/* below here only when crossing rings, such as from user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
} __attribute__((packed));
```

# 代码

TRAPHANDLER 和 TRAPHANDLER_NOEC 这两个宏帮助定义了有错误码和无错误码的中断处理函数，使这些函数压入中断号的同时保持栈结构的统一、跳转 _alltraps，这些函数在 trap_init() 中被设置为 IDT 表项中的 offset。

_alltraps 保存 es 和 ds、通用寄存器，加载新的 es 和 ds，压入参数并调用 trap。

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

trap_init() 建立了 IDT 表，在进入内核时由i386_init()调用
其中，仅在处理系统调用时，还允许开中断。
```
// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
    //   see section 9.6.1.3 of the i386 reference: "The difference between
    //   an interrupt gate and a trap gate is in the effect on IF (the
    //   interrupt-enable flag). An interrupt that vectors through an
    //   interrupt gate resets IF, thereby preventing other interrupts from
    //   interfering with the current interrupt handler. A subsequent IRET
    //   instruction restores IF to the value in the EFLAGS image on the
    //   stack. An interrupt through a trap gate does not change IF."
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//	  the privilege level required for software to invoke
//	  this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, dpl)            \
{                               \
    (gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;     \
    (gate).gd_sel = (sel);                  \
    (gate).gd_args = 0;                 \
    (gate).gd_rsv1 = 0;                 \
    (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;    \
    (gate).gd_s = 0;                    \
    (gate).gd_dpl = (dpl);                  \
    (gate).gd_p = 1;                    \
    (gate).gd_off_31_16 = (uint32_t) (off) >> 16;       \
}

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
  SETGATE(idt[3], 0, GD_KT, th3, 0);
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


