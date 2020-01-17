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
初始化一个类似于页管理结构数组 pages[] 的环境管理结构数组 envs[]，见 exercise1。

## 1.3 Creating and Running Environments

## 1.4 Handling Interrupts and Exceptions

## 1.5 Basics of Protected Control Transfer

## 1.6 Types of Exceptions and Interrupts

## 1.7 An Example

## 1.8 Nested Exceptions and Interrupts

## 1.9 Setting Up the IDT

# 2 Part B: Page Faults, Breakpoints Exceptions, and System Calls

## 2.1 Handling Page Faults

## 2.2 The Breakpoint Exception

## 2.3 System calls

## 2.4 User-mode startup

## 2.5 Page faults and memory protection




