# Lab 3: User Environments

# 0 Introduction
这一部分涉及用户环境的管理。

具体内容包括：设置管理用户环境的数据结构、创建一个用户环境、装载并运行一个程序镜像，以及完善系统调用等异常的处理。

JOS 中的 environment 大致等于 UNIX 中的 process ，是程序的一次执行。

## 0.1 Getting Started
切换到 lab3 的分支：
```
athena% cd ~/6.828/lab
athena% add git
athena% git commit -am 'changes to lab2 after handin'
athena% git pull
athena% git checkout -b lab3 origin/lab3
athena% git merge lab2
athena% git diff lab2
```
Lab3 新增源文件：
```
inc/
env.h	Public definitions for user-mode environments
trap.h	Public definitions for trap handling
syscall.h	Public definitions for system calls from user environments to the kernel
lib.h	Public definitions for the user-mode support library

kern/
env.h	Kernel-private definitions for user-mode environments
env.c	Kernel code implementing user-mode environments
trap.h	Kernel-private trap handling definitions
trap.c	Trap handling code
trapentry.S	Assembly-language trap handler entry-points
syscall.h	Kernel-private definitions for system call handling
syscall.c	System call implementation code

lib/
Makefrag	Makefile fragment to build user-mode library, obj/lib/libjos.a
entry.S	Assembly-language entry-point for user environments
libmain.c	User-mode library setup code called from entry.S
syscall.c	User-mode system call stub functions
console.c	User-mode implementations of putchar and getchar, providing console I/O
exit.c	User-mode implementation of exit
panic.c	User-mode implementation of panic

user/	*	Various test programs to check kernel lab 3 code
```

# 1 Part A: User Environments and Exception Handling
在文件 inc/env.h 中包含了用户环境的基本定义。

内核使用 Env 数据结构来表示每个用户环境。在本实验中只会创建一个最初的用户环境，但在 Lab4 中将会 fork 出其他用户环境。

在文件 kern/env.c 中，内核维护了3个全局变量：
```
struct Env *envs = NULL;		// All environments
struct Env *curenv = NULL;		// The current env
static struct Env *env_free_list;	// Free environment list
```

## 1.1 Environment State
结构 Env 的定义在 inc/env.h ：
```
struct Env {
	struct Trapframe env_tf;	// Saved registers
	struct Env *env_link;		// Next free Env
	envid_t env_id;			// Unique environment identifier
	envid_t env_parent_id;		// env_id of this env's parent
	enum EnvType env_type;		// Indicates special system environments
	unsigned env_status;		// Status of the environment
	uint32_t env_runs;		// Number of times environment has run

	// Address space
	pde_t *env_pgdir;		// Kernel virtual address of page dir
};
```
env_tf:定义在 inc/trap.h，保存陷入内核态时的寄存器内容。

env_type:用于区分特殊的环境。大部分的环境类型都为 ENV_TYPE_USER。

env_status:状态包括 ENV_FREE、ENV_RUNNABLE、ENV_RUNNING、ENV_NOT_RUNNABLE、ENV_DYING（僵尸环境在下一次陷入内核时被释放）。

env_pgdir:保存页目录的虚拟地址。

用户环境伴随着 thread 和 address space 的概念：thread 表示 env_tf 中保存的寄存器，address space 表示 env_pgdir 指向的页目录和页表。用户环境只有被内核设置好 thread 和 address space 后才能运行。

与 XV6 的 proc 结构不同的是，JOS 中的 env 没有自己独立的内核栈，因为在 JOS 中，一次只能有一个用户环境处于内核态，而 XV6 中可以有多个 proc 处于内核态（阻塞、切换、并发）。

## 1.2 Allocating the Environments Array

> Exercise1. 在 mem_init() 中初始化一个类似于页管理结构数组 pages[] 的环境管理结构数组 envs[]。

## 1.3 Creating and Running Environments

这部分将初始化并运行第一个用户环境。由于还没有实现文件系统，所以将会使用预先嵌入在内核的 ELF 格式二进制程序镜像。

Lab 3的 GNUmakefile 将会产生许多二进制程序镜像在 obj/user/ 目录。

在文件 kern/Makefrag 中，使用` -b binary `选项，链接器会将这些二进制镜像与 .o 文件一样直接与内核程序链接。

在文件 obj/kern/kernel.sym 中，链接器产生的这类奇怪符号 _binary_obj_user_hello_start, _binary_obj_user_hello_end, 以及 _binary_obj_user_hello_size 可以使得内核程序能够引用这些嵌入内核的二进制文件。

> Exercise 2. 完善 env.c 中用以设置用户环境的函数。

进入内核后的调用路径如下：
```
start (kern/entry.S)
i386_init (kern/init.c)
	cons_init
	mem_init
	env_init
	trap_init (still incomplete at this point)
	env_create
	env_run
		env_pop_tf
```

如果一切正常，编译运行后系统将进入用户空间执行 hello 二进制程序，并在该程序中使用 int 指令执行系统调用。

由于目前的 JOS 还没有设置好硬件以实现从用户空间到内核空间的陷入机制，当 CPU 发现它无法处理这个系统调用时，它将会产生一个 general protection exception，再次发现不能处理，产生一个 double fault exception，仍然不能处理，产生一个被称为 triple fault 的错误然后放弃。

这时，CPU 通常会重置并重启系统，这种机制对历史遗留的应用很重要但对内核开发来说十分麻烦，因此本实验定制的 QEMU 版本中，会出现 register dump 以及"Triple fault."信息。

从内核进入用户空间、再进入内核的关键点如下：
```
在 env_pop_tf 函数的 `iret` 指令后就会真正进入用户模式
用户程序的第一条指令是 `cmpl`（位于 lib/entry.S 中的 start 部分）
在 hello 程序中的 sys_cputs() 使用了 int $0x30 指令，该指令是一个输出字符到控制台的系统调用
```

目前可以使用 GDB 来检查我们已经进入了用户模式：
在 env_pop_tf 处设置断点，使用 `si` 单步遍历该函数，见到用户程序的第一条指令 `cmpl`。在 sys_cputs() 中的 int $0x30 处设置断点（地址位于 obj/user/hello.asm）。
```
The target architecture is assumed to be i8086
[f000:fff0]    0xffff0: ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb) b env_pop_tf                //设置断点
Breakpoint 1 at 0xf0102d5f: file kern/env.c, line 470.
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0xf0102d5f <env_pop_tf>: push   %ebp

Breakpoint 1, env_pop_tf (tf=0xf01b2000) at kern/env.c:470
470 {
(gdb) si                        //单步
=> 0xf0102d60 <env_pop_tf+1>:   mov    %esp,%ebp
0xf0102d60  470 {
(gdb)                           //单步
=> 0xf0102d62 <env_pop_tf+3>:   sub    $0xc,%esp
0xf0102d62  470 {
(gdb)                           //单步
=> 0xf0102d65 <env_pop_tf+6>:   mov    0x8(%ebp),%esp
471     asm volatile(
(gdb)                           //单步
=> 0xf0102d68 <env_pop_tf+9>:   popa   
0xf0102d68  471     asm volatile(
(gdb)                           //单步
=> 0xf0102d69 <env_pop_tf+10>:  pop    %es
0xf0102d69 in env_pop_tf (tf=<error reading variable: Unknown argument list address for `tf'.>)
    at kern/env.c:471
471     asm volatile(
(gdb)                           //单步
=> 0xf0102d6a <env_pop_tf+11>:  pop    %ds
0xf0102d6a  471     asm volatile(
(gdb)                           //单步
=> 0xf0102d6b <env_pop_tf+12>:  add    $0x8,%esp
0xf0102d6b  471     asm volatile(
(gdb)                           //单步
=> 0xf0102d6e <env_pop_tf+15>:  iret   
0xf0102d6e  471     asm volatile(
(gdb) info registers            //在执行iret前，查看寄存器信息
eax            0x0  0
ecx            0x0  0
edx            0x0  0
ebx            0x0  0
esp            0xf01b2030   0xf01b2030
ebp            0x0  0x0
esi            0x0  0
edi            0x0  0
eip            0xf0102d6e   0xf0102d6e <env_pop_tf+15>
eflags         0x96 [ PF AF SF ]
cs             0x8  8        //0x8正是内核代码段的段选择子
ss             0x10 16
ds             0x23 35
es             0x23 35
fs             0x23 35
gs             0x23 35
(gdb) si                          //单步执行，指令应该执行iret指令
=> 0x800020:    cmp    $0xeebfe000,%esp
0x00800020 in ?? ()
(gdb) info registers              //执行iret指令后，差看寄存器
eax            0x0  0
ecx            0x0  0
edx            0x0  0
ebx            0x0  0
esp            0xeebfe000   0xeebfe000
ebp            0x0  0x0
esi            0x0  0
edi            0x0  0
eip            0x800020 0x800020
eflags         0x2  [ ]
cs             0x1b 27      //0x18是用户代码段在GDT中的偏移，用户权限是0x3，所以选择子正好是0x1b
ss             0x23 35     //这些寄存器值都是在env_alloc()中被设置好的
ds             0x23 35
es             0x23 35
fs             0x23 35
gs             0x23 35
(gdb) b *0x800a1c              //通过查看obj/user/hello.asm找到断点位置
Breakpoint 2 at 0x800a1c
(gdb) c
Continuing.
=> 0x800a1c:    int    $0x30   //系统调用指令，现在还不起作用

Breakpoint 2, 0x00800a1c in ?? ()
(gdb) 
```

观察执行iret前后的cs段寄存器的值，执行iret前cs的值0x8正是内核代码段的段选择子（GD_KT定义在inc/memlayout.h中），执行后cs的值0x1b，0x18是用户代码段的在GDT中的偏移（GD_UT定义在inc/memlayout.h中），用户权限是0x3，所以选择子正好是0x1b。

env_alloc()中对 tf 的关键设置：
```
int
env_alloc(struct Env **newenv_store, envid_t parent_id)
{
    ……
    // Allocate and set up the page directory for this environment.
    if ((r = env_setup_vm(e)) < 0)
        return r;

	……

    // Clear out all the saved register state,
    // to prevent the register values
    // of a prior environment inhabiting this Env structure
    // from "leaking" into our new environment.
    memset(&e->env_tf, 0, sizeof(e->env_tf));

    // Set up appropriate initial values for the segment registers.
    // GD_UD is the user data segment selector in the GDT, and
    // GD_UT is the user text segment selector (see inc/memlayout.h).
    // The low 2 bits of each segment register contains the
    // Requestor Privilege Level (RPL); 3 means user mode.  When
    // we switch privilege levels, the hardware does various
    // checks involving the RPL and the Descriptor Privilege Level
    // (DPL) stored in the descriptors themselves.
    e->env_tf.tf_ds = GD_UD | 3;    //设置ds，段选择子（在 gdt 中的偏移 + RPL）
    e->env_tf.tf_es = GD_UD | 3;    //设置es
    e->env_tf.tf_ss = GD_UD | 3;    //设置ss
    e->env_tf.tf_esp = USTACKTOP;   //设置esp
    e->env_tf.tf_cs = GD_UT | 3;    //设置cs
    // You will set e->env_tf.tf_eip later.
	
	……
}
```

## 1.4 Handling Interrupts and Exceptions

> Excersize 3. 熟悉 x86 中断异常机制。

## 1.5 Basics of Protected Control Transfer
中断和异常都是“受保护的控制权转移”，它们能在防止用户程序触碰内核或其他用户环境的情况下，使处理器从用户态进入内核态。

在 intel 的术语中，中断是处理器外部事件造成的受保护控制转移，如 I/O 中断；异常来自处理器内部，如除零异常。

为了确保控制权的转移是受保护的，两个机制确保了中断异常发生时只能固定地进入预设的内核入口：
```
1.The Interrupt Descriptor Table. 
x86 允许了从0到255的256个中断向量，以此查找 IDT。
根据IDT描述符可以获取中断处理函数cs和eip的值，从而进入中断处理函数执行。

2.The Task State Segment
当x86异常发生，并且发生了从用户模式到内核模式的转换时，处理器也会进行栈切换，从而将用户上下文寄存器信息保存到一个安全的地方。
TSS中的ESP0和SS0两个字段指定了内核栈的位置。
TSS是一个很大的数据结构，但 JOS 中使用它来定位内核栈。
那么内核如何找到这个TSS结构的呢？同样是查找GDT。在trap_init_percpu()函数中，设置了 gdt 中的TSS，并使用ltr指令设置TSS选择子到寄存器：
void
trap_init_percpu(void)
{
    // Setup a TSS so that we get the right stack
    // when we trap to the kernel.
    ts.ts_esp0 = KSTACKTOP;
    ts.ts_ss0 = GD_KD;
    ts.ts_iomb = sizeof(struct Taskstate);
    // Initialize the TSS slot of the gdt.
    gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
                    sizeof(struct Taskstate) - 1, 0);
    gdt[GD_TSS0 >> 3].sd_s = 0;
    // Load the TSS selector (like other segment selectors, the
    // bottom three bits are special; we leave them 0)
    ltr(GD_TSS0);       //设置TSS选择子
    // Load the IDT
    lidt(&idt_pd);
}
```

总结各种寄存器：

![图1](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab3/images/1.png?raw=true)

## 1.6 Types of Exceptions and Interrupts
x86 处理器内部产生的同步异常使用 0-31 号中断向量，例如缺页异常使用 14 号。

大于 31 号的中断向量，可以被 int 指令产生（软件中断），也可被处理器外部的异步硬件中断产生。

这一部分将处理 0-31 号内部异常，下一节处理 48 号系统调用，Lab4 处理外部硬件中断。

## 1.7 An Example
假设处理器正在执行代码，这时遇到一条除法指令尝试除以0，处理器将会做如下动作：
1. 将栈切换到TSS的SS0和ESP0字段定义的内核栈中，在JOS中两个值分别是GD_KD和KSTACKTOP。
2. 处理器在内核栈中压入如下参数：
```
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | old SS   |     " - 4
                     |      old ESP       |     " - 8
                     |     old EFLAGS     |     " - 12
                     | 0x00000 | old CS   |     " - 16
                     |      old EIP       |     " - 20 <---- ESP 
                     +--------------------+    
```
3. 除以0的异常中断号是0，处理器读取IDT的第0项，从中解析出CS:EIP。
4. CS:EIP处的异常处理函数执行。

对于一些异常来说，除了压入上图五个word，还会压入错误代码，如下所示：
```
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | old SS   |     " - 4
                     |      old ESP       |     " - 8
                     |     old EFLAGS     |     " - 12
                     | 0x00000 | old CS   |     " - 16
                     |      old EIP       |     " - 20
                     |     error code     |     " - 24 <---- ESP
                     +--------------------+   
```
压入的数据和`Trapframe`结构是一致的。

## 1.8 Nested Exceptions and Interrupts
如果中断发生时，处理器已经是内核态（CS 寄存器中的低2位为0），就不会发生内核栈的切换。

这时，如果由于某些原因（如栈空间不够），无法将旧的寄存器信息压栈，那么处理器只能重置。不用说，内核需要处理这种情况。

## 1.9 Setting Up the IDT
头文件 inc/trap.h 和 kern/trap.h 包含了重要的相关定义。kern/trap.h 中都是内核私有的定义，而 inc/trap.h 包含了用户程序也可能使用的定义。

大致的异常控制流如下：
```
      IDT                   trapentry.S         trap.c
   
+----------------+                        
|   &handler1    |---------> handler1:          trap (struct Trapframe *tf)
|                |             // do stuff      {
|                |             call trap          // handle the exception/interrupt
|                |             // ...           }
+----------------+
|   &handler2    |--------> handler2:
|                |            // do stuff
|                |            call trap
|                |            // ...
+----------------+
       .
       .
       .
+----------------+
|   &handlerX    |--------> handlerX:
|                |             // do stuff
|                |             call trap
|                |             // ...
+----------------+
```
trap_init() 将会初始化 IDT。
每个 handler 都应在栈中构建出一个 struct Trapframe，然后调用 trap()。trap() 将会按类型分发或处理这些中断异常。

> Exercise 4. 完善 trapentry.S，trap.c 中的相关部分。

Questions
> 为什么要给每个中断/异常单独一个处理函数？

跳转路径：IDT -> individual handler -> alltraps -> trap。

虽然最终都会跳转到 alltraps 和 trap，但独立的 handler 可以压入中断号（硬件不会自动压入），并使不产生错误码的中断保持栈的结构一致，这两个作用无法由统一的 alltraps 和 trap 发挥。

# 2 Part B: Page Faults, Breakpoints Exceptions, and System Calls
现在内核已具备了初步处理异常的能力，还需要在 trap 调用的 trap_dispatch 函数中根据具体的异常类型进行处理。

## 2.1 Handling Page Faults

> Exercise 5. 在函数 trap_dispatch 中分发 14 号中断 Page Faults。

## 2.2 The Breakpoint Exception
3 号中断 breakpoint exception，通常用来使 debuggers 能够在程序中插入断点，实现的方式是在程序中放置 int 3 指令。

在 JOS 中，使任何用户态下也能主动从 breakpoint exception 进入内核（伪系统调用），并且在进入 JOS 的内核 monitor，这种用法相当于把 monitor 视作了一个 primitive debugger。例如在 lib/panic.c 的用户模式的  panic() 中，就使用了这一机制。

> Exercise 6. 在函数 trap_dispatch 中使 3 号中断 breakpoint exception 能够进入内核的 monitor。

## 2.3 System calls
JOS 使用中断号 48 (0x30) 作为系统调用。

使用寄存器传递系统调用号和参数：系统调用号保存在%eax，五个参数依次保存在%edx, %ecx, %ebx, %edi, %esi中。返回值保存在%eax中。

> Exercise 7. 完善系统调用机制。

系统调用流程回顾，以user/hello.c为例：
```
调用了lib/print.c中的cprintf,，该cprintf()最终会调用lib/syscall.c中的sys_cputs()
sys_cputs()又会调用lib/syscall.c中的syscall()，该函数传参、引发陷入
发生中断后，硬件去IDT中查找中断处理函数，进入内核，最终会走到kern/trap.c的trap_dispatch()
根据中断号0x30，又会调用kern/syscall.c中的syscall()函数
在该函数中根据系统调用号调用kern/print.c中的cprintf()函数，该函数最终调用kern/console.c中的cputchar()将字符串打印到控制台
当trap_dispatch()返回后，trap()会调用 env_run(curenv)，回到用户程序系统调用之后的那条指令运行，且寄存器eax中保存着系统调用的返回值

```

现代系统中，使用 intel 特别定制的 sysenter/sysexit 指令，能够比传统的 int/iret 指令更快执行系统调用。

## 2.4 User-mode startup
用户程序总是从 lib/entry.S 开始启动，在进行一些设置后，调用 lib/libmain.c 中的 libmain()。

在 libmain() 中进行一些设置后，调用用户程序的 umain()，umain()返回后 libmain() 调用 lib/exit.c 中的 exit 优雅退出，而 exit 实际上使用了系统调用 sys_env_destroy。

> Exercise 8. 完善 lib/libmain.c 中的 libmain()。

## 2.5 Page faults and memory protection
操作系统依赖处理器来实现内存保护。当程序试图访问无效地址或没有访问权限时，处理器在当前指令停住，引发中断进入内核。如果内核能够修复，则在刚才的指令处继续执行，否则程序将无法接着运行。

而系统调用为内存保护带来了问题。大部分系统调用接口让用户程序传递一个指针参数给内核。这些指针指向的是用户缓冲区。通过这种方式，系统调用在执行时就可以解引用这些指针。但是这里有两个问题：
1. 在内核中的page fault要比在用户程序中的page fault更严重。如果内核在操作自己的数据结构时出现 page faults，这是一个内核的bug，而且异常处理程序会中断整个内核。但是当内核在解引用由用户程序传递来的指针时，它需要一种方法去记录此时出现的任何page faults都是由用户程序带来的。
2. 内核通常比用户程序有着更高的内存访问权限。用户程序很有可能要传递一个指针给系统调用，这个指针指向的内存区域是内核可以进行读写的，但是用户程序不能。此时内核必须小心的去解析这个指针，否则的话内核的重要信息很有可能被泄露。

综上：

如果页错误发生在内核态时应该直接 panic ；应该对使用了指针参数的系统调用进行内存权限检查。

> Exercise 9. 完善上述保护机制。

> Exercise 10. 与实验9类似。

# 3 Conclusion

这一部分涉及用户环境的管理。

主要内容包括：设置管理用户环境的数据结构、创建第一个用户环境并装载运行第一个程序镜像，完善系统调用等异常的处理。

## 3.1 用户环境/进程

1. 内核维护一个名叫envs的Env数组，每个Env结构对应一个进程，Env结构最重要的字段有Trapframe env_tf（该字段中断发生时可以保持寄存器的状态），pde_t *env_pgdir（该进程的页目录地址）。

2. 定义了env_init()，env_create()等函数，初始化Env结构，将Env结构Trapframe env_tf中的寄存器值设置到寄存器中，从而执行该Env。

## 3.2 中断/异常机制

1. 创建异常处理函数，建立并加载IDT，使JOS能支持中断处理。内核态和用户态转换方式：通过中断机制可以从用户环境进入内核态。使用iret指令从内核态回到用户环境。

2. 完善 Page Faults, Breakpoints Exceptions, 以及 System Calls 这三种异常发生时的处理函数。



