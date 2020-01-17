# Lab 2: Memory Management

# 0 Introduction

内存管理主要有两个组件。

第一个是内核使用的物理内存分配器，用以分配和回收物理空间，它的操作单元是一页（4096B）。
需要实现的内容包括：
1. 数据结构用以记录空闲物理页、已分配页被多个进程共享的情况
2. 分配和回收物理页的方法
   
第二个是虚拟内存机制，用以将虚拟地址转换为物理地址。在实际指令执行时MMU会根据页表执行地址转换的工作。
所以需要实现的内容：
1. 符合特定规范的页表

需要首先浏览以下源文件：
- inc/memlayout.h
- kern/pmap.c
- kern/pmap.h
- kern/kclock.h
- kern/kclock.c
- inc/mmu.h

memlayout.h 描述了需要实现的虚拟地址布局。

memlayout.h 和 pmap.h 定义了物理内存分配器使用的 PageInfo 结构。

kclock.c and kclock.h 管理PC的时钟、CMOS RAM 硬件，这个硬件记录了PC的物理空间总数（以及其他信息），pmap.c 的代码需要读取这些内容。

inc/mmu.h 也包含了许多有用的定义。

# 1 Part 1: Physical Page Management
实现物理内存分配器。

数据结构主要是全体物理页信息结构表pages、空闲物理页链表page_free_list。

方法包括物理内存初始化mem_init()/boot_alloc()、分配器初始化page_init()、页面分配和回收方法page_alloc()/page_free()等。

> Exercise 1. 完善 kern/pmap.c 中的函数，实现物理内存分配器。

# 2 Part 2: Virtual Memory

> Exercise 2. 阅读 x86 手册，理解页寻址、基于分页的保护机制、段寻址、基于分段的保护机制。

## 2.1 Virtual, Linear, and Physical Addresses
地址转换示意：
```

           Selector  +--------------+         +-----------+
          ---------->|              |         |           |
                     | Segmentation |         |  Paging   |
Software             |              |-------->|           |---------->  RAM
            Offset   |  Mechanism   |         | Mechanism |
          ---------->|              |         |           |
                     +--------------+         +-----------+
            Virtual                   Linear                Physical
```
**一个 C pointer 就是上述virtual address 中的 "offset"。**

在 boot/boot.S 中，我们初始化了GDT，即将所有的段基址和界限分别设为 0 和 0xffffffff。
因此，上述的"selector"部分没有作用，the linear address 总是等于 virtual address 中的 "offset"。

在JOS中，定义了类型 uintptr_t 来表示 opaque 的虚拟地址，类型 physaddr_t 来表示物理地址，它们都是32-bit integers (uint32_t) 的同义词。
如果直接 dereference uintptr_t ，则编译器会报错，因为它们都是整型。
可以直接把 uintptr_t 强制类型转换为 pointer type 然后再dereference，但不能对 physaddr_t 做同样的操作。

总结：
```
C type	Address type
T*  	Virtual
uintptr_t  	Virtual
physaddr_t  	Physical
```
JOS将虚拟地址0xf0000000映射到物理地址0x0处的一个原因就是希望能有一个简便的方式实现物理地址和线性地址的转换。
有时需要读写物理地址的内容，可以通过 KADDR(pa) 来 add 0xf0000000 得到相应的虚拟地址。
同理，为了由虚拟地址得到物理地址，通过  PADDR(va) 来实现。

> Exercise 3. 使用 GDB 来查看对应虚存和物理内存的内容。

## 2.2 Reference counting
经常出现多个虚拟地址（多个页表）映射到同一个物理地址的情况，所以在 struct PageInfo 中记录每个物理页的引用数 pp_ref 。

一般情况下，（用户页）引用值等于该物理页出现在所有页表的 UTOP 地址以下的次数，因为在 UTOP 地址以上的映射关系（内核页）大多数已在 boot 时由 kernel 设置好了（即在boot_map_region函数中）并且不应被释放，所以没必要记录它们的引用值（其作用主要是判断某页是否应该释放）。

注意 page_alloc 返回的空闲页的 pp_ref 总是为0，所以调用者或其他函数需要增加引用值。

## 2.3 Page Table Management
这一部分实现页表管理的相关内容。

数据结构主要是内核页目录kern_pgdir，涉及页目录和页表条目类型pte_t/pde_t。

方法包括查询页目录页表获得页表条目pgdir_walk()、建立和撤销地址映射boot_map_region()/page_insert()/page_remove()、获得虚拟地址所在页的信息结构page_lookup()。

> Exercise 4. 实现 kern/pmap.c 中建立映射所需的函数。

# 3 Part 3: Kernel Address Space

## 3.1 Permissions and Fault Isolation
JOS将线性地址空间分为两部分，由定义在inc/memlayout.h中的ULIM分割，大约给内核保留了256MB虚拟地址空间。

ULIM以上的部分，属于内核空间，用户没有权限访问，内核有读写权限。

[UTOP,ULIM)的部分，用户和内核都只能读，因为这部分是用来使用户能看到部分内核的只读数据结构的。

UTOP以下的部分，属于用户空间，用户和内核都能读写。

访问权限的机制通过软硬件配合实现，MMU转换地址时读取pte或pde的perm控制位来判断权限。

```
/*
 * Virtual memory map:                                Permissions
 *                                                    kernel/user
 *
 *    4 Gig -------->  +------------------------------+
 *                     |                              | RW/--
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     :              .               :
 *                     :              .               :
 *                     :              .               :
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| RW/--
 *                     |                              | RW/--
 *                     |   Remapped Physical Memory   | RW/--
 *                     |                              | RW/--
 *    KERNBASE, ---->  +------------------------------+ 0xf0000000      --+
 *    KSTACKTOP        |     CPU0's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                   |
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     |     CPU1's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                 PTSIZE
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     :              .               :                   |
 *                     :              .               :                   |
 *    MMIOLIM ------>  +------------------------------+ 0xefc00000      --+
 *                     |       Memory-mapped I/O      | RW/--  PTSIZE
 * ULIM, MMIOBASE -->  +------------------------------+ 0xef800000
 *                     |  Cur. Page Table (User R-)   | R-/R-  PTSIZE
 *    UVPT      ---->  +------------------------------+ 0xef400000
 *                     |          RO PAGES            | R-/R-  PTSIZE
 *    UPAGES    ---->  +------------------------------+ 0xef000000
 *                     |           RO ENVS            | R-/R-  PTSIZE
 * UTOP,UENVS ------>  +------------------------------+ 0xeec00000
 * UXSTACKTOP -/       |     User Exception Stack     | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebff000
 *                     |       Empty Memory (*)       | --/--  PGSIZE
 *    USTACKTOP  --->  +------------------------------+ 0xeebfe000
 *                     |      Normal User Stack       | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebfd000
 *                     |                              |
 *                     |                              |
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     .                              .
 *                     .                              .
 *                     .                              .
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 *                     |     Program Data & Heap      |
 *    UTEXT -------->  +------------------------------+ 0x00800000
 *    PFTEMP ------->  |       Empty Memory (*)       |        PTSIZE
 *                     |                              |
 *    UTEMP -------->  +------------------------------+ 0x00400000      --+
 *                     |       Empty Memory (*)       |                   |
 *                     | - - - - - - - - - - - - - - -|                   |
 *                     |  User STAB Data (optional)   |                 PTSIZE
 *    USTABDATA ---->  +------------------------------+ 0x00200000        |
 *                     |       Empty Memory (*)       |                   |
 *    0 ------------>  +------------------------------+                 --+
 *
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.  JOS user programs map pages temporarily at UTEMP.
 */
```

## 3.2 Initializing the Kernel Address Space
设置UTOP以上的地址空间。

> Exercise 5. 完善mem_init()中初始化内核页目录中的内核部分空间。

## 3.3 Question
1. JOS支持的最大物理空间是多少？4G。因为地址为32位，页目录10位、页表10位、页偏移12位。
2. 如果支持最大的物理空间，需要多少空间用以管理物理空间？需要的管理空间为：1个页目录 + 1024个页表，即 1 * 4K + 1024 * 4K 。
3. 如何分解这些管理结构所占的空间？JOS中，1个页目录初始即被分配，而页表所占的空间是在需要时才会被分配。
4. 在kern/entry.S and kern/entrypgdir.c中，开启分页机制后EIP仍处于低地址空间，通过jmp指令后才转换到了高地址空间，为什么能这样做？因为初始页表中设置了虚拟地址 [KERNBASE, KERNBASE+4MB) 和 [0, 4MB) 到 物理空间 [0, 4MB) 的映射。

## 3.4 Address Space Layout Alternatives

虚拟地址空间的布局不是唯一的。JOS中，user的地址空间包含Kernel和user两部分，kernel没有独立的地址空间（也没有独立的进程）。

OS可以将kernel部分映射在低虚拟地址空间，而把user部分放在高虚拟地址空间。但基于x86的OS通常不会这样做，因为x86的一个兼容模式virtual 8086 mode，会使用低虚拟地址空间。

OS也可以设计将整个4G虚拟地址空间都给user使用。
思路：
1. 修改JOS使kernel有自己独立的页表，并且user的页表中只保留进入和离开kernel的部分（切换页表？）。
2. 这时必须有机制保证kernel能够读写系统调用的参数、user的地址空间。
3. 这样做肯定会带来页表切换的开销。

## 4 Other Challenges

1. JOS的物理空间管理粒度为4KB的一页，但有时可能需要大于4KB的连续物理空间。可以修改原来的内存分配机制，使一页的大小可以在2的幂范围变化，并且大页可以分解为小页，小页也可以合并为大页。（这样可能出现碎片问题？）
2. 使kernel部分的物理页能够开启4MB的超级页模式，以减小页表中映射kernel部分的空间。（设置pde和pte中的PTE_PS控制位）
3. 增加JOS kernel monitor的指令：展示给定虚拟地址范围内对应的页面映射信息（pte的条目：实际的物理页、物理地址、权限）；能够修改任何页面映射的权限；输出给定地址范围内的实际内容。

## 5 conclusion
该实验大体上做三件事：
1. 提供管理物理内存的数据结构和函数、初始化物理空间
2. 提供修改页目录和页表树结构的函数，从而达到虚拟页到物理页映射的目的
3. 用前面两部分的函数建立内核的线性地址空间，即设置页表的内容。

