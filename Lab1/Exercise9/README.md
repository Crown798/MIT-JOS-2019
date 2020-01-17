>Exercise 9. Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

问题：kernel的栈在何时、何地、如何被初始化？栈指针的初始位置？

# 结论
entry.S 77行初始化栈

栈的位置是0xf0108000-0xf0110000

设置栈的方法是在kernel的数据段预留32KB空间(entry.S 92行)

栈顶的初始化位置是0xf0110000

# 分析
entry.S 77行设置栈指针的代码如下：
```
    # Set the stack pointer
    movl    $(bootstacktop),%esp
```
从kern/kernel文件中找出符号bootstacktop的位置：
```
$ objdump -D obj/kern/kernel | grep -3 bootstacktop
f0108000 <bootstack>:
        ...

f0110000 <bootstacktop>:
f0110000:       01 10                   add    %edx,(%eax)
f0110002:       11 00                   adc    %eax,(%eax)
        ...
```
堆栈的大小由下面的指令设置(entry.S 92行):
```
.data
###################################################################
# boot stack
###################################################################
	.p2align	PGSHIFT		# force page alignment
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
bootstacktop:
```
可以看出，栈的设置方法是在数据段中预留出一些空间来用作栈空间。
memlayout.h 97行定义的栈的大小:
```
#define PGSIZE      4096        // bytes mapped by a page
...
#define KSTKSIZE    (8*PGSIZE)          // size of a kernel stack
```
因此栈大小为32KB，栈的位置为0xf0108000-0xf0110000。