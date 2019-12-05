>Exercise 10. To become familiar with the C calling conventions on the x86, find the address of the test_backtrace function in obj/kern/kernel.asm, set a breakpoint there, and examine what happens each time it gets called after the kernel starts. How many 32-bit words does each recursive nesting level of test_backtrace push on the stack, and what are those words?

在test_backtrace函数入口设置断点，检验每次该函数被调用后栈中内容的变化，每次压入了多少32位的字？这些字是什么内容？

# 分析
test_backtrace的源代码与反汇编：
```
test_backtrace(int x)
{
        cprintf("entering test_backtrace %d\n", x);
        if (x > 0)
                test_backtrace(x-1);
        else
                mon_backtrace(0, 0, 0);
        cprintf("leaving test_backtrace %d\n", x);
}

f0100040:       55                      push   %ebp                             ;压入调用函数的%ebp
f0100041:       89 e5                   mov    %esp,%ebp                        ;将当前%esp存到%ebp中，作为栈帧
f0100043:       53                      push   %ebx                             ;保存%ebx当前值，防止寄存器状态被破坏
f0100044:       83 ec 14                sub    $0x14,%esp                       ;开辟20字节栈空间用于本函数内使用
f0100047:       8b 5d 08                mov    0x8(%ebp),%ebx                   ;取出调用函数传入的第一个参数
f010004a:       89 5c 24 04             mov    %ebx,0x4(%esp)                   ;压入cprintf的最后一个参数，x的值
f010004e:       c7 04 24 e0 19 10 f0    movl   $0xf01019e0,(%esp)               ;压入cprintf的倒数第二个参数，指向格式化字符串"entering test_backtrace %d\n"
f0100055:       e8 27 09 00 00          call   f0100981 <cprintf>               ;调用cprintf函数，打印entering test_backtrace (x)
f010005a:       85 db                   test   %ebx,%ebx                        ;测试是否小于0
f010005c:       7e 0d                   jle    f010006b <test_backtrace+0x2b>   ;如果小于0，则结束递归，跳转到0xf010006b处执行
f010005e:       8d 43 ff                lea    -0x1(%ebx),%eax                  ;如果不小于0，则将x的值减1，复制到栈上
f0100061:       89 04 24                mov    %eax,(%esp)                      ;接上一行
f0100064:       e8 d7 ff ff ff          call   f0100040 <test_backtrace>        ;递归调用test_backtrace
f0100069:       eb 1c                   jmp    f0100087 <test_backtrace+0x47>   ;跳转到f0100087执行
f010006b:       c7 44 24 08 00 00 00    movl   $0x0,0x8(%esp)                   ;如果x小于等于0，则跳到这里执行，压入mon_backtrace的最后一个参数
f0100072:       00 
f0100073:       c7 44 24 04 00 00 00    movl   $0x0,0x4(%esp)                   ;压入mon_backtrace的倒数第二个参数
f010007a:       00 
f010007b:       c7 04 24 00 00 00 00    movl   $0x0,(%esp)                      ;压入mon_backtrace的倒数第三个参数
f0100082:       e8 68 07 00 00          call   f01007ef <mon_backtrace>         ;调用mon_backtrace，这是这个练习需要实现的函数
f0100087:       89 5c 24 04             mov    %ebx,0x4(%esp)                   ;压入cprintf的最后一个参数，x的值
f010008b:       c7 04 24 fc 19 10 f0    movl   $0xf01019fc,(%esp)               ;压入cprintf的倒数第二个参数，指向格式化字符串"leaving test_backtrace %d\n"
f0100092:       e8 ea 08 00 00          call   f0100981 <cprintf>               ;调用cprintf函数，打印leaving test_backtrace (x)
f0100097:       83 c4 14                add    $0x14,%esp                       ;回收开辟的栈空间
f010009a:       5b                      pop    %ebx                             ;恢复寄存器%ebx的值
f010009b:       5d                      pop    %ebp                             ;恢复寄存器%ebp的值
f010009c:       c3                      ret                                     ;函数返回
```
一个栈帧(stack frame)的大小计算如下：

push %ebp，将上一个栈帧的地址压入，增加4字节

push %ebx，保存ebx寄存器的值，增加4字节

sub $0x14, %esp，开辟20字节的栈空间，后面的函数调用传参直接操作这个栈空间中的数，而不是用pu sh的方式压入栈中

在执行call test_backtrace时压入这条指令下一条指令的地址，压入4字节返回地址

加起来一共是32字节，也就是8个int。

# 第一次调用分析 
以第一调用栈为例分析，32个字节代码的含义如下图所示：
```
0xf010ffb0:     0x00000004      0x00000005      0x00000000      0x00010094
0xf010ffc0:     0x00010094      0x00010094      0xf010fff8      0xf0100144
             +--------------------------------------------------------------+
             |    next x    |     this x     |  don't know   |  don't know  |
             +--------------+----------------+---------------+--------------+
             |  don't know  |    last ebx    |  last ebp     | return addr  |
             +------ -------------------------------------------------------+
```
按照理论分析，一个完整的调用栈最少需要的字节数等于4+4+4+4*3=24字节，即返回地址，上一个函数的ebp，保存的ebx，函数内没有分配局部变量，需要再加12个字节用来调用mon_backtrace时传参数。

为什么要分配32字节？有一个说法是，因为x86的栈大小必须是16的整数倍。

# 数据对齐和栈对齐
>数据对齐。许多计算机系统对基本数据类型的合法地址做了一些限制，要求某种类型对象的地址必须是某个值K的倍数，其中K的大小为该数据类型的大小。

举个实际的例子：比如我们在内存中读取一个8字节长度的变量，那么这个变量所在的地址必须是8的倍数。如果这个变量所在的地址是8的倍数，那么就可以通过一次内存操作完成该变量的读取。倘若这个变量所在的地址并不是8的倍数，那么可能就需要执行两次内存读取，因为该变量被放在两个8字节的内存块中了。这种对齐限制简化了形成处理器和内存系统之间接口的硬件设计。

无论数据是否对齐，x86_64硬件都能正常工作，但是却会降低系统的性能，所以编译器在编译时一般会为我们实施数据对齐。

>栈的字节对齐，实际是指栈顶指针必须须是16字节的整数倍。

栈对齐帮助在尽可能少的内存访问周期内读取数据，不对齐堆栈指针可能导致严重的性能下降。

https://www.cnblogs.com/tcctw/p/11333743.html