# Lab 1: 
# 1 Part 1: PC Bootstrap
这一部分讲解 PC 启动的相关部分。

## 1.1 x86 assembly
两种风格的汇编语言：Intel syntax 和 AT&T syntax。

Intel syntax 参考：
- [PC Assembly Language Book](https://pdos.csail.mit.edu/6.828/2018/readings/pcasm-book.pdf) 
- [80386 Programmer's Reference Manual](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm)
- [IA-32 Intel Architecture Software Developer's Manuals](https://software.intel.com/en-us/articles/intel-sdm)

AT&T syntax 参考：[Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html)

GNU环境使用AT&T syntax。

> Exercise 1. 熟悉汇编语言。

## 1.2 Simulating the x86
实验环境准备：配置虚拟机、设置编译工具链、用QEMU模拟x86硬件。

> Excersise 0. 环境搭建。

## 1.3 The PC's Physical Address Space
```
+------------------+  <- 0xFFFFFFFF (4GB)
|      32-bit      |
|  memory mapped   |
|     devices      |
|                  |
/\/\/\/\/\/\/\/\/\/\

/\/\/\/\/\/\/\/\/\/\
|                  |
|      Unused      |
|                  |
+------------------+  <- depends on amount of RAM
|                  |
|                  |
| Extended Memory  |
|                  |
|                  |
+------------------+  <- 0x00100000 (1MB)
|     BIOS ROM     |
+------------------+  <- 0x000F0000 (960KB)
|  16-bit devices, |
|  expansion ROMs  |
+------------------+  <- 0x000C0000 (768KB)
|   VGA Display    |
+------------------+  <- 0x000A0000 (640KB)
|                  |
|    Low Memory    |
|                  |
+------------------+  <- 0x00000000
```
最早期的16-bit Intel 8088处理器仅支持1MB（0x00000000~0x000FFFFF）的物理寻址能力。底下640KB空间称为低地址空间，是早期PC的RAM占用的内存空间；剩下384KB空间有特殊用途（设备映射）：video display buffers / BIOS / firmware held in non-volatile memory。

到了80286和80386处理器，分别支持16MB和4GB的物理寻址能力。为了做到向后兼容，保留了低1MB的内存布局，使得8086的设备仍然能够映射到0x000A0000 ~ 0x00100000这个地址空间。在4GB的地址空间，BIOS等32位设备位于4GB地址空间的顶部。

现在IA64 CPU支持的地址空间超过4GB，因此又保留了32为设备映射区域一，向后兼容留给32位的设备映射。所以现代的计算机的地址空间中会存在两个洞。

JOS仅会使用PC的前256MB物理空间，设备仍然映射到低1MB的空间。

## 1.4 The ROM BIOS
开机启动首先执行BIOS，此时计算机处于实模式。为了向后兼容，让8086上的软件能够继续运行，新的x86处理器都保留了开机运行在实模式下。

开机后第一条指令在`0xf000:0xfff0`处，该条指令为`ljmp $0xf000,$0xe05b`，跳转到BIOS的前半部分。

然后 BIOS **设定了中断描述符表，初始化VGA显示器等设备**。在初始化完PCI总线和所有BIOS负责的重要设备后，它就开始搜索软盘、硬盘、或是CD-ROM等可启动的设备。最终，当它找到可引导磁盘时，BIOS从磁盘**读取引导加载程序并将控制权转移给它**。

> Exercise 2. 使用GDB的si指令来跟踪BIOS的前几条指令，弄清楚BIOS一开始运行时做了什么。

# 2 Part 2: The Boot Loader
这一部分讲解 boot loader 的相关内容。

## 2.1 boot loader的主要工作
- 加载全局描述符表，从实模式**进入保护模式**（boot/boot.S）
- 从磁盘**加载kernel到内存**（boot/main.c）

> Exercise 3. 使用 GDB 跟踪遍历 boot loader。

> Exercise 4. 掌握 C 语言的指针相关内容。

## 2.2 源文件boot/boot.S
```
  cli                         # Disable interrupts
  cld                         # String operations increment
  # Set up the important data segment registers (DS, ES, SS).
  xorw    %ax,%ax             # Segment number zero
  movw    %ax,%ds             # -> Data Segment
  movw    %ax,%es             # -> Extra Segment
  movw    %ax,%ss             # -> Stack Segment
```
进入boot loader后执行的第一条指令位于0x7c00处，即`cli`。
```
  # Enable A20:
  #   For backwards compatibility with the earliest PCs, physical
  #   address line 20 is tied low, so that addresses higher than
  #   1MB wrap around to zero by default.  This code undoes this.
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60
```
这几行主要是为了开启A20，也就是处理器的第21根地址线。在早期8086处理器上每次到物理地址达到最高端的0xFFFFF时，再加1，就又会绕回到最低地址0x00000，当时很多程序员会利用这个特性编写代码，但是到了80286时代，处理器有了24根地址线，为了保证之前编写的程序还能运行在80286机子上，设计人员默认关闭了A20，需要我们自己打开，这样就解决了兼容性问题。
```
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0
```
`lgdt`这条指令的格式是`lgdt m48`，操作数是一个48位的内存区域，该指令将这6字节加载到全局描述表寄存器（GDTR）中，低16位是全局描述符表（GDT）的界限值，高32位是GDT的基地址。”gdtdesc“被定义在第82行：
```
gdt:
  SEG_NULL              # null seg
  SEG(STA_X|STA_R, 0x0, 0xffffffff) # code seg
  SEG(STA_W, 0x0, 0xffffffff)           # data seg

gdtdesc:
  .word   0x17                            # sizeof(gdt) - 1
  .long   gdt                             # address gdt
```
可以看到GDT有3项，第一项是空项，第二第三项分别是代码段，数据段，它们的起始地址都是0x0，段界限都是0xffffffff。如此，线性地址最终等于逻辑地址，绕开了x86的分段机制。

`lgdt`指令后面的三行是将CR0寄存器第一位置为1，其他位保持不变，这将导致处理器的运行变成保护模式。
```
  # Set up the stack pointer and call into C.
  movl    $start, %esp
  call bootmain
```
接下来设置栈指针，然后调用bootmain函数。

## 2.3 ELF概述
ELF格式中大多数复杂的部分是为了支持动态链接。

从linking的角度：ELF header、Section Header Table、多个Section。**从execution的角度：ELF header、Program Header Table、多个Segment。**
Segment本质上是从装载的角度重新划分了ELF的各个Section。目标文件链接成可执行文件时，链接器会尽可能把相同权限属性的段（Section）分配到同一Segment。

C定义的ELF headers在inc/elf.h中。
Section header 列出了每个需要装入的sections。
通常使用的program sections是：
- .text: 程序的可执行指令
- .rodata: read-only-data，C编译器产生的ASCII字符串常量
- .data: 保存程序初始化数据的数据分区(data section)，如全局变量的初始化int x=6;

当链接器计算一个程序的内存布局的时候，会为未初始化的全局变量保存空间在.bss数据区中，在内存中位于.data之后。C要求未初始化的全局变量默认初始化为0，因此.bss中不会存储任何内容，链接器会记录.bss的大小和内存地址（记录在program header中？），装入器或程序本身负责清零.bss区的数据。

查看kernel的所有sections的names, sizes, and link addresses：
```
objdump -h obj/kern/kernel
```
>注意到kernel的 .text section 的"VMA" (or link address)和"LMA" (or load address)不相同。通过虚拟内存机制来满足VMA。

>The load address of a section is the memory address at which that section should be loaded into memory.The link address of a section is the memory address from which the section expects to execute.如果代码的实际执行地址不是VMA，就会出错。

> BIOS将boot loader载入0x7c00，所以这就是boot loader的load address。这也是它应该开始执行的地方，也就是它的link address。为了设置这个link address，在 boot/Makefrag中告诉连接器 `-Ttext 0x7C00`。

> Exercise 5. 使用 GDB 验证 boot laoder 的 link address。

查看kernel的program headers：
```
objdump -x obj/kern/kernel
```
查看kernel的entry point：
```
objdump -f obj/kern/kernel
```

## 2.4 源文件boot/main.c
```
    struct Proghdr *ph, *eph;

    // read 1st page off disk
    readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);

    // is this a valid ELF?
    if (ELFHDR->e_magic != ELF_MAGIC)
        goto bad;

    // load each program segment (ignores ph flags)
    ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph++)
        // p_pa is the load address of this segment (as well
        // as the physical address)
        readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

    // call the entry point from the ELF header
    // note: does not return!
    ((void (*)(void)) (ELFHDR->e_entry))();
```
`void readseg(uint32_t pa, uint32_t count, uint32_t offset)`函数从磁盘offset字节(offset相对于第一个扇区第一个字节开始算)对应的扇区开始读取count字节到物理内存pa处。

所以以上代码首先读取第一个扇区的SECTSIZE*8（一页）字节的内核文件（ELF格式）到物理内存ELFHDR（0x10000）处。接下来检查ELF文件的魔数。接下来从ELF文件头读取ELF Header的e_phoff和e_phnum字段，分别表示Segment结构在ELF文件中的偏移，和项数。然后将每一个Segment从ph->p_offset对应的扇区读到物理内存ph->p_pa处。

将内核ELF文件中的Segment从磁盘全部读取到内存后，跳转到ELFHDR->e_entry指向的指令处。正式进入内核代码中。

这一步执行完后CPU，内存，磁盘可以抽象如下：

![图1](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab1/images/1.png?raw=true)

> Exercise 6. 使用 GDB 验证 boot loader 将 kernel 装入了地址 0x100000 处。

# 3 Part 3: The Kernel
这部分讲解内核启动部分、标准输出库函数、函数调用对栈的应用。

## 3.1 开启分页模式
操作系统经常被加载到高虚拟地址处，比如0xf0100000，但是并不是所有机器都有这么大的物理内存。可以使用内存管理硬件做到将高地址虚拟地址映射到低地址物理内存。

/kern/entry.S：
```
    movl    $(RELOC(entry_pgdir)), %eax
    movl    %eax, %cr3          //cr3 寄存器保存页目录表的物理基地址
    # Turn on paging.
    movl    %cr0, %eax
    orl $(CR0_PE|CR0_PG|CR0_WP), %eax
    movl    %eax, %cr0          //cr0 的最高位PG位设置为1后，正式打开分页功能
```
RELOC这个宏的定义如下：`#define RELOC(x) ((x) - KERNBASE)`，KERNBASE又被定义在/inc/memlayout.h中：`#define KERNBASE 0xF0000000`。因为现在还没开启分页模式，而在链接时链接器根据/kern/kernel.ld中的内容指定entry_pgdir这个符号代表的地址以0xF0000000为基址，通过RELOC(entry_pgdir)得到页目录实际的物理地址。

entry_pgdir定义在/kern/entrypgdir.c中：
```
__attribute__((__aligned__(PGSIZE)))        //强制编译器分配给entry_pgdir的空间地址是4096(一页大小)对齐的
pde_t entry_pgdir[NPDENTRIES] = {           //页目录表。这是uint32_t类型长度为1024的数组
    // Map VA's [0, 4MB) to PA's [0, 4MB)
    [0]
        = ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P,    //设置页目录表的第0项
    // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
    [KERNBASE>>PDXSHIFT]
        = ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P + PTE_W //设置页目录表的第KERNBASE>>PDXSHIFT(0xF0000000>>22)项
};
```
可见entry_pgdir中将虚拟地址[0, 4MB)映射到物理地址[0, 4MB)，以及[0xF0000000, 0xF0000000+4MB)映射到[0, 4MB）。

所以以上代码将页目录的物理地址复制到cr3寄存器，并且将cr0 的最高位PG位设置为1后，正式打开分页功能。

> Exercise 7. 使用 GDB 验证开启分页功能的语句。

## 3.2 格式化输出到控制台
本小节关注格式化输出以及控制台的相关函数，代码位于kern/printf.c, lib/printfmt.c, kern/console.c中。重点函数为vprintfmt、cga_putc。

存在如下调用关系：

![图2](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab1/images/2.png?raw=true)

### 3.2.1 kern/console.c
本文件中包括底层硬件相关的Serial I/O ， Parallel port output，CGA/VGA display output，Keyboard input部分，中层device-independent console部分，高层console I/O部分。
对外接口函数主要如下：
```
void
cputchar(int c)
{
    cons_putc(c);
}
static void
cons_putc(int c)
{
    serial_putc(c);
    lpt_putc(c);
    cga_putc(c);
}
static void
cga_putc(int c)
{
    // if no attribute given, then use black on white
    if (!(c & ~0xFF))
        c |= 0x0700;

    switch (c & 0xff) {
    case '\b':
        if (crt_pos > 0) {
            crt_pos--;
            crt_buf[crt_pos] = (c & ~0xff) | ' ';
        }
        break;
    case '\n':                  //如果遇到的是换行符，将光标位置下移一行，也就是加上80（每一行占80个光标位置）
        crt_pos += CRT_COLS;
        /* fallthru */
    case '\r':                  //如果遇到的是回车符，将光标移到当前行的开头，也就是crt_post-crt_post%80
        crt_pos -= (crt_pos % CRT_COLS);
        break;
    case '\t':                  //制表符很显然
        cons_putc(' ');
        cons_putc(' ');
        cons_putc(' ');
        cons_putc(' ');
        cons_putc(' ');
        break;
    default:                    //普通字符的情况，直接将ascii码填到显存中
        crt_buf[crt_pos++] = c;     /* write the character */
        break;
    }

    // What is the purpose of this?
    if (crt_pos >= CRT_SIZE) {      //判断是否需要滚屏。文本模式下一页屏幕最多显示25*80个字符，
        int i;                      //超出时，需要将2~25行往上提一行，最后一行用黑底白字的空白块填充

        memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
            crt_buf[i] = 0x0700 | ' ';
        crt_pos -= CRT_COLS;
    }

    /* move that little blinky thing */     //移动光标
    outb(addr_6845, 14);
    outb(addr_6845 + 1, crt_pos >> 8);
    outb(addr_6845, 15);
    outb(addr_6845 + 1, crt_pos);
}
```
cga_putc()函数**将int c打印到控制台**，可以看到该函数处理会打印正常的字符外，还能处理回车换行等控制字符，甚至还能处理滚屏。该函数会将字符对应的ascii码存储到crt_buf[crt_pos]处，实际上crt_buf在初始化的时候被初始为KERNBASE(0xF00B8000) + CGA_BUF(0xB8000)，也就是虚拟地址0xF00B8000处，这里正是显存的起始地址（物理地址0xB8000）。***所以往控制台写字符串，本质还是往物理地址0xB8000开始的显存写数据。***

### 3.2.2 lib/printfmt.c
真正实现字符串输出的是vprintfmt()函数，其他函数都是对它的包装：
```
void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
    ……
	while (1) {
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0')
				return;
			putch(ch, putdat);
		}

		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {
            ……
		}
	}
}
```
vprintfmt()函数很长，大的框架是一个while循环，while循环中首先会处理常规字符，然后对于格式化的处理使用switch语句。

> Exercise 8. 找到并完善格式化输出函数，使之可以输出八进制数。

遇到的问题：
>函数不定参数与省略号

“…”告诉编译器，在函数调用时不检查形参类型是否与实参类型相同，也不检查参数个数。

获取可变参数时需要使用宏va_begin()、va_end()、va_arg()。

这些宏的原理，就是根据参数入栈的特点从最靠近第一个可变参数的固定参数开始，依次获取每个可变参数的地址。

为了知道每个可变参数占据的大小，需要确定每个可变参数的类型，所以要在固定参数中包含足够的信息让程序可以确定每个可变参数的类型。比如，printf，程序通过分析format字符串就可以确定每个可变参数的类型。

### 3.2.3 kern/printf.c
本文件中主要是对printfmt.c和console.c中函数的封装。

遇到的问题：
>#define NULL ((void *) 0)。

在c语言中，0是一个特殊的值，它可以表示：**整型数值0，空字符，逻辑假**（false）。表示的东西多了，有时候不好判断。尤其是空字符和数字0之间。

为了明确的指出，0是空字符的含义，用到了： ((void *) 0) 这个表达式。表示把0强制转换为空字符。

## 3.3 The Stack
GCC 将 C 语言程序编译为汇编语言程序时，会遵循一定的规范。例如，函数调用前会将参数入栈，被调函数会将 ebp 入栈。

**the way the C language uses the stack on the x86：**
- 执行call指令前，函数调用者将参数入栈，按照函数列表从右到左的顺序入栈
- call指令会自动将当前eip（函数返回地址）入栈，并设置eip为被调用函数地址
- 被调用函数负责：开始时将ebp入栈，esp的值赋给ebp；结束时ebp的值赋给esp，将ebp出栈

例子：

![图3](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab1/images/3.png?raw=true)

> Excercise 9. kernel的栈在何时、何地、如何被初始化？栈指针的初始位置？

> Excercise 10. 在test_backtrace函数入口设置断点，检验每次该函数被调用后栈中内容的变化.

> Excercise 11. Excercise 12. 补全回溯函数mon_backtrace()。

