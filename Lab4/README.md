# Lab 4：Preemptive Multitasking

# 0 Introduction
这一部分涉及多用户环境间的抢占式调度。

Part A 的内容包括：多处理器支持、round-robin 调度、涉及
用户环境管理的系统调用（创建/销毁/分配内存等）。

Part B 的内容包括：用户级别的 fork()，允许用户环境创建一个副本。

Part C 的内容包括：时钟中断处理、抢占式调度、进程间通信 IPC。

## 0.1 Getting Started
切换到 lab4 的分支：
```
athena% cd ~/6.828/lab
athena% add git
athena% git pull
athena% git checkout -b lab4 origin/lab4
athena% git merge lab3
...
```
Lab4 新增源文件：
```
kern/cpu.h Kernel-private definitions for multiprocessor support
kern/mpconfig.c Code to read the multiprocessor configuration
kern/lapic.c Kernel code driving the local APIC unit in each processor
kern/mpentry.S Assembly-language entry code for non-boot CPUs
kern/spinlock.h Kernel-private definitions for spin locks, including the big kernellock
kern/spinlock.c Kernel code implementing spin locks
kern/sched.c Code skeleton of the scheduler that you are about to implement
```

# 1 Part A: Multiprocessor Support and Cooperative Multitasking
Part A 的内容包括：多处理器支持、非抢占式 round-robin 调度、涉及用户环境管理的系统调用（创建/销毁PCB、分配内存等）。

## 1.1 Multiprocessor Support
我们将使 JOS 支持 symmetric multiprocessing(SMP) 模型，即所有的 CPU 对内存和I/O总线等系统资源有同等使用权。

在系统启动阶段，有一个 bootstrap processor(BSP) 首先负责启动并初始化 OS，然后再启动其他的 application processors (APs)。硬件和 BIOS 来决定哪个处理器作为 BSP。

每个 CPU 都有自己的局部 APIC 单元(LAPIC) ，负责传递中断。在文件 kern/lapic.c 中定义了如下函数以使用 LAPIC：
- cpunum() 通过 LAPIC 的 ID 来判断当前代码运行在哪个 CPU
- lapic_startap() 从 BSP 向 APs 传递 STARTUP 中断来启动处理器
- apic_init()使用 LAPIC 的内置时钟产生时钟中断，以支持抢占式调度

处理器通过 MMIO 技术来访问 LAPIC。在 MMIO 中，一部分物理内存被 hardwired to I/O设备的寄存器，所以用于访问内存的 load/store 指令也能用于访问设备的寄存器。

类似于开始于物理地址 0xA0000 处的 I/O hole，LAPIC 占用的 I/O hole 开始于物理地址 0xFE000000 (32MB short of 4GB)。JOS 将其映射在 MMIOBASE 开始的 4MB 地址空间。

>Exercise 1.在 kern/pmap.c 中实现 MMIO 部分的映射 mmio_map_region()。

### 1.1.1 Application Processor Bootstrap
准备启动 APs 前，BSP 首先收集多处理器系统的信息：CPU 总数、各自的 APIC IDs、LAPIC unit 的 MMIO 地址。文件 kern/mpconfig.c 中的 mp_init() 通过读取 BIOS 部分内存的 MP configuration table 来收集这些信息。

之后， kern/init.c 文件中的 boot_aps() 开始启动 AP：首先将 kern/mpentry.S 中的 AP entry code（类似 boot/boot.S 中的 bootloader）复制到物理地址 0x7000 (MPENTRY_PADDR)，然后一个一个地向 AP 的 LAPIC 单元发送 STARTUP 中断信号以及 AP 开始运行的初始物理地址 CS:IP ( MPENTRY_PADDR in our case)并等待。

当前 AP 在实模式下运行 kern/mpentry.S：切换到开启分页的保护模式，然后跳转 kern/init.c 中的 C 代码 mp_main()，该函数为当前 AP 设置 GDT，TSS等，最后设置cpus数组中当前CPU对应的结构的cpu_status为CPU_STARTED。

当检测到当前 AP 的 CPU_STARTED 设置后，boot_aps() 才开始启动下一个 AP。

>Exercise 2. 阅读 kern/init.c 中的 boot_aps() and mp_main() 以及 kern/mpentry.S，理解 APs 启动过程。修改 kern/pmap.c 中的 page_init() 来防止初始化物理页空闲队列时错误地回收  AP bootstrap code 占用的 MPENTRY_PADDR 处的页面。

问题：
>注意到 kern/mpentry.S 像其他内核代码一样编译链接运行在 KERNBASE 之上，而其装入地址为 MPENTRY_PADDR，所以其 the link address and the load address 不相同。

CPU 启动控制流总结：
1. BP：boot/boot.S（开启A20、切换保护模式（装初始GDT、设置初始段寄存器）） -> boot/main.c/bootmain()（装入内核） -> kern/entry.S（使用entry_pgdir、开启分页、初始化栈） -> kern/init.c/i386_init()（cons_init、mem_init、env_init、trap_init、mp_init、lapic_init、pic_init、boot_aps）
2. APs：kern/mpentry.S（切换保护模式（装初始GDT、设置初始段寄存器）、使用entry_pgdir、开启分页、初始化栈） -> kern/init.c/mp_main()（使用kern_pgdir、lapic_init、env_init_percpu、trap_init_percpu）

### 1.1.2 Per-CPU State and Initialization
文件 kern/cpu.h 中定义的 struct CpuInfo 结构存储了每个 CPU 的私有信息：
1. Per-CPU kernel stack. 虽然 JOS 只允许同时仅一个用户环境在内核态运行，但每个处理器都有独立的内核栈防止互相干扰（因为进入trap()后才申请内核锁，而此时已向内核栈压入了信息）。数组 percpu_kstacks[NCPU][KSTKSIZE] 保留了内核栈的物理空间，被映射到虚存 KSTACKTOP 处。
2. Per-CPU TSS and TSS descriptor. 用于指明内核栈所在。TSS 存放在 cpus[i].cpu_ts，相应的 TSS descriptor 存放在 gdt[(GD_TSS0 >> 3) + i]。
3. Per-CPU current environment pointer. 这时符号 curenv 指向 cpus[cpunum()].cpu_env 或 thiscpu->cpu_env。
4. Per-CPU system registers. 比如cr3, gdt, ltr这些寄存器都是每个CPU私有的，函数env_init_percpu() and trap_init_percpu() 被用来初始化这些寄存器。
```
struct CpuInfo {
    uint8_t cpu_id;                 // Local APIC ID; index into cpus[] below
    volatile unsigned cpu_status;   // The status of the CPU
    struct Env *cpu_env;            // The currently-running environment.
    struct Taskstate cpu_ts;        // Used by x86 to find stack for interrupt
};
```

>Exercise 3. 修改 kern/pmap.c 中的 mem_init_mp() 来映射 per-CPU stacks。

>Exercise 4. 修改 kern/trap.c 中的 trap_init_percpu() 来使之适合初始化所有 CPU 的 TSS and TSS descriptor。

### 1.1.3 Locking
JOS 使用一个 big kernel lock 来解决多 CPU 使用内核代码的竞争条件问题。当任意用户环境进入内核态时都需要申请内核锁，并在返回用户态时释放。如此，同时仅有一个用户环境可在内核态下运行。

文件 kern/spinlock.h 定义了内核锁 kernel_lock，并提供了方法 lock_kernel() and unlock_kernel()。

需要在以下几处运用内核锁：
1. i386_init(),在 BSP 唤醒其他 CPU 之前加锁
2. mp_main(), 当前 AP 在执行 sched_yield() 前需要加锁
3. trap()，从用户模式陷入内核时加锁
4. env_run()，在刚好切换回用户态时（env_pop_tf 前）释放锁

>Exercise 5. 如上所述，添加内核锁。

现代 OS 使用一种 fine-grained locking 来允许部分内核态下的并发运行。典型的需加锁内核部件如下：
```
The page allocator.
The console driver.
The scheduler.
The inter-process communication (IPC) state
```

## 1.2 Round-Robin Scheduling
下一步实现非抢占式 round-robin 调度。

文件 kern/sched.c 中的 sched_yield() 按环形方式搜索 envs[] 数组找到下一个处于 ENV_RUNNABLE 的用户环境，并调用 env_run() 切换之。

用户环境通过系统调用 sys_yield() 来使用内核函数 sched_yield()，自愿放弃 CPU。

>Exercise 6. 实现 sched_yield()，并在 syscall() 中分发 sys_yield() 系统调用。

注意函数 env_run() 不会返回，所以用户进程调用 sched_yield() 下 CPU 后再次被切换上 CPU 时不会原路返回紧接 sched_yield() 剩余部分运行，而是同样通过 env_run() 重新运行，上下文信息为 env 结构中的 tf 信息。
```
void
env_run(struct Env *e)
{
	if(curenv && curenv->env_status == ENV_RUNNING)
		curenv->env_status = ENV_RUNNABLE;
	curenv = e;
	curenv->env_status = ENV_RUNNING;
	curenv->env_runs++;
	lcr3(PADDR(e->env_pgdir));

	env_pop_tf(&e->env_tf);
}
```

注意进程切换时，上下文的保存过程：
```
上下文保存在 env 结构中的 tf 结构（非指针）。（在 xv6 中由于每个进程有自己的内核栈，所以 pcb 中的 tf 是指向内核栈某部分的指针）
从用户态陷入内核态时，将内核栈中保存的上下文信息，保存到了 env 结构中，所以进程切换时无需再次保存用户态上下文：
void
trap(struct Trapframe *tf)
{
    ……
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
    ……
}
```

## 1.3 System Calls for Environment Creation
尽管现在内核有能力在多进程之前切换，但是仅限于内核创建的用户进程。目前JOS还没有提供系统调用，使用户进程能创建新的进程。

Unix提供fork()系统调用创建新的进程，fork()拷贝父进程的地址空间和寄存器状态到子进程。父进程从fork()返回的是子进程的进程ID，而子进程从fork()返回的是0。父进程和子进程有独立的地址空间，任何一方修改了内存，不会影响到另一方。

JOS 将会提供更加细化的用户进程创建原语集（一堆系统调用），并使用它们来实现一种用户层级的 fork()。

这些系统调用包括：
1. sys_exofork()：创建一个新的进程，地址空间仅映射内核部分，寄存器状态和父环境一致。在父进程中sys_exofork()返回新进程的envid或错误码，子进程返回0。
2. sys_env_set_status：设置一个特定进程的状态为ENV_RUNNABLE或ENV_NOT_RUNNABLE。
3. sys_page_alloc：为特定进程分配一个物理页，并映射到给定虚地址。
4. sys_page_map：拷贝另一进程的某一页映射关系。
5. sys_page_unmap：解除一页映射关系。

使用 kern/env.c 中的 envid2env() 可方便从 envid 获得 env 结构，且 0 表示 当前的 env 。

>Exercise 7. 实现以上系统调用。

用户测试文件 user/dumbfork.c 使用以上系统调用来实现了一个类似 Unix 的 fork() 的用户函数。

# 2 Part B: Copy-on-Write Fork
早期 Unix 的 fork() 将会完全复制一份父进程的地址空间的内容给子进程，而子进程被 fork 之后往往会跟着调用一个 exec()，用新程序来替换地址空间的内容，这使得 fork() 所做的大部分工作被浪费。

后来的 Unix 使用了写时复制技术：fork() 只会拷贝地址空间的映射关系而非实际的页面内容，并在父子进程的 pte 中将共享的页面标记为只读。当父子进程中的任一个需要写受保护的页面时，通过缺页异常来分配并映射一个新的物理页到原虚拟地址，然后再写入内容。

接下来将实现一个用户级别的写时复制 fork()。

## 2.1 User-level page fault handling
写时复制 fork() 的实现需要缺页中断机制的支持。

传统的 Unix 的缺页中断机制中，内核根据缺页异常发生的位置来采取相应的措施。例如，缺页发生在 stack region 将分配并映射新的一页，发生在 BSS region 会分配清零并映射新的一页，发生在 text region 会从磁盘中读入一页相应的二进制内容并映射。

这种机制需要内核跟踪许多信息。所以，JOS 在用户空间进行缺页中断的处理，由用户决定如何应对。

### 2.1.1 Setting the Page Fault Handler
JOS 由用户决定如何处理缺页异常，所以需要通过 sys_env_set_pgfault_upcall 系统调用来注册一个处理入口，该信息记录在 env 结构的 env_pgfault_upcall。

>Exercise 8. 实现 sys_env_set_pgfault_upcall 系统调用。

### 2.1.2 Normal and Exception Stacks in User Environments
当缺页中断发生时，内核会返回用户模式来处理该中断，所以需要一个用户异常栈来模拟内核栈。

JOS的用户异常栈被定义在虚拟地址UXSTACKTOP，大小也是一页，使用前需要调用 sys_page_alloc() 来分配一页物理空间。

### 2.1.3 Invoking the User Page Fault Handler
按如下逻辑，修改 kern/trap.c 中的页异常内核处理函数（从用户陷入内核时调用）：
```
1. 判断curenv->env_pgfault_upcall是否设置，若没有已注册的页异常用户处理函数，则销毁用户环境
2. 修改esp，切换到用户异常栈（若当前已为用户异常栈，则往当前 tf->tf_esp 之后继续压入内容）
3. 在栈上压入一个UTrapframe结构：

                    <-- UXSTACKTOP
trap-time esp
trap-time eflags
trap-time eip
trap-time eax       start of struct PushRegs
trap-time ecx
trap-time edx
trap-time ebx
trap-time esp
trap-time ebp
trap-time esi
trap-time edi       end of struct PushRegs
tf_err (error code)
fault_va            <-- %esp when handler is run

4. 将eip设置为curenv->env_pgfault_upcall，然后回到用户态执行curenv->env_pgfault_upcall处的代码
```

注意 UTrapframe 结构中包含的 tf 信息，能够用来恢复出现页异常时的用户态。

>Exercise 9. 完善 kern/trap.c 中的 page_fault_handler ，使其引出用户注册的页异常处理函数。

### 2.1.4 User-mode Page Fault Entrypoint
现在需要完善用户页异常处理的汇编部分，这一部分负责调用用户页异常处理的 C 语言部分，然后恢复出现页异常时的用户态。

>Exercise 10. 完善 lib/pfentry.S 中的 _pgfault_upcall。

然后需要完善用户页异常处理的 C 语言部分，这一部分是用户决定的发生用户态页异常时实际的处理逻辑。

这里首先完成注册用户页异常处理 C 语言部分的库函数，下一节实现实际的处理函数。

>Exercise 11. 完善 lib/pgfault.c 中的 set_pgfault_handler()。

用户级别的缺页异常处理总结：
1. 缺页发生，陷入内核：trap()->trap_dispatch()->page_fault_handler()
2. page_fault_handler()重回用户态：切换用户异常栈，压入UTrapframe结构，然后调用curenv->env_pgfault_upcall
3. _pgfault_upcall 汇编函数 调用 _pgfault_handler C函数执行实际的处理，然后恢复出现页异常时的用户态。

## 2.2 Implementing Copy-on-Write Fork
现在可以实现用户级别的写时复制 fork 了。

文件 lib/fork.c 中的 fork() 逻辑如下：
1. 使用 set_pgfault_handler() 设置用户页异常处理的 C 语言部分
2. 调用 sys_exofork() 创建一个 Env 结构（复制了父进程的寄存器状态、分配了一个已映射 UTOP 以上部分的页目录）
3. 调用 duppage 拷贝 UTOP 以下部分的地址空间：其中对于可写或本身标记写时复制的页，父子进程的 pte 都应标记 PTE_COW 位，以区分正常的只读页。特殊的是，用户异常栈的空间不能被这样拷贝，需要独自分配一页空间给子进程，因为缺页异常处理本身就需要使用异常栈。
4. 为子进程设置_pgfault_upcall
5. 将子进程状态设置为ENV_RUNNABLE。

用户页异常处理的 C 语言部分，逻辑如下：
1. 判断是页错误（错误码是FEC_WR），且该页的 PTE_COW 位是1，否则 panic
2. 分配一个新的物理页，并拷贝之前出现错误的页的内容，然后重新映射新的物理页到出错地址所在页

进程的页表被映射在地址 UVPT 以方便用户能够读取。页表自映射机制如下：利用了硬件的固定寻址机制，即固定的三次计算 pd = lcr3(); pt = *(pd+4*PDX); page = *(pt+4*PTX)，从而找到某一个 page table 所在的物理页。https://pdos.csail.mit.edu/6.828/2018/labs/lab4/uvpt.html。如图：

![图2](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab4/images/2.png?raw=true)

>Exercise 12. 完善 lib/fork.c 中的 fork、duppages、pgfault 函数。

# 3 Part C: Preemptive Multitasking and Inter-Process communication (IPC)
这一部分实现抢占式调度，以及 IPC。

## 3.1 Clock Interrupts and Preemption
在当前的系统中，用户环境开始执行后可以一直占用 CPU，这不利于系统安全。所以需要支持时钟中断这样的外部硬件中断，使内核能重获 CPU 的控制权。

### 3.1.1 Interrupt discipline
外部硬件中断被称为 IRQs，共计 16 种。在 picirq.c 的 pic_init 函数中将 IRQs 0-15 映射到了 IRQ_OFFSET - IRQ_OFFSET+15，而在 inc/trap.h 中，IRQ_OFFSET 被定义为 32，所以 IDT 的 32-47 号条目对应 IRQs 0-15 。其中，时钟中断为 IRQ 0，即对应 IDT[IRQ_OFFSET+0]。

相较于 XV6，JOS 中做了一个关键的简化：在内核中总是关中断的，而在用户态是开中断。中断的开关由 %eflags 寄存器的 FL_IF 位表示，置为 1 则开中断。

所以，需要保证用户环境在进入内核后关中断，恢复用户态后开中断。

>Exercise 13. 完善 kern/trapentry.S and kern/trap.c 中硬件中断相关的陷入机制，并修改 kern/env.c 的 env_alloc() 保证进程运行在开中断下。

### 3.1.2 Handling Clock Interrupts
文件 init.c 中的 i386_init 会调用 lapic_init and pic_init ，它们会初始化时钟的相关设置，以周期性产生时钟中断。

现在需要编写时钟中断的处理逻辑，即切换进程。

>Exercise 14. 完善 trap_dispatch() 以处理时钟中断。

## 3.2 Inter-Process communication (IPC)
通过已实现的虚存机制 + 陷入机制，JOS 已给用户环境提供了重要的隔离特性，而另一个重要的待提供的特性则是通信。

进程间通信有许多种 models ，而在 JOS 中仅实现了一种简单的 IPC 机制（共享内存）。

### 3.2.1 IPC in JOS
JOS 的 IPC 机制提供的接口：两个系统调用 sys_ipc_recv and sys_ipc_try_send，以及两个包装库函数 ipc_recv and ipc_send。

用户间通信的一个 message 的内容包括：一个 32 位值，一个可选的页映射（一次可分享一页的信息）。

### 3.2.2 Sending and Receiving Messages
进程调用 sys_ipc_recv 以接收一个 message：该系统调用阻塞当前进程，直到接收任意进程向其发送的一个 message。

进程调用 sys_ipc_try_send 以发送一个 message 给特定的进程：如果目标进程正在等待接收消息（调用了 sys_ipc_recv 并还未接收消息），则发送成功并返回 0，否则发送失败并返回 -E_IPC_NOT_RECV。

库函数 ipc_recv 负责调用 sys_ipc_recv 并从 struct Env 中抽取出接收到的信息。

库函数 ipc_send 负责持续调用 sys_ipc_try_send 直到发送信息成功。

### 3.2.3 Transferring Pages
若调用 sys_ipc_recv 时传入了参数 dstva ，调用 sys_ipc_try_send 时传入了参数 srcva ，则经过映射后最终两个进程共享同一个物理页面。

若没有这两个参数或参数无效，则不会共享页面。

### 3.2.4 Implementing IPC

>Exercise 15. 完善 kern/syscall.c 的 sys_ipc_recv and sys_ipc_try_send ，以及 lib/ipc.c 的 ipc_recv and ipc_send。



