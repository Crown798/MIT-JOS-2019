>Exercise 6. We can examine memory using GDB's x command. The GDB manual has full details, but for now, it is enough to know that the command x/Nx ADDR prints N words of memory at ADDR. (Note that both 'x's in the command are lowercase.) Warning: The size of a word is not a universal standard. **In GNU assembly, a word is two bytes** (the 'w' in xorw, which stands for word, means 2 bytes).

>Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.)

要求： 使用 GDB 验证 boot loader 将 kernel 装入了地址 0x100000 处。

# 结论
当BIOS进入bootloader时，因为此时工作在实模式，0x10000以上的内存根本无法访问，所以内存中的内容应该为空。

因为内核的代码段会加载到内存地址0x100000，当bootloader进入kernel时，程序地址0x100000处为加载的kernel的代码。

# qemu验证
使用断点使程序停止在0x7c00处，即进入bootloader时，并检验内存地址0x100000：
```
(gdb) b *0x7c00
Breakpoint 1 at 0x7c00
(gdb) c
Continuing.
[   0:7c00] => 0x7c00:  cli    

Breakpoint 1, 0x00007c00 in ?? ()
(gdb) x /16xb 0x100000
0x100000:   0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x100008:   0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
```
使用断点使程序停止在0x76d1处，即即将进入内核入口时，并检验内存地址0x100000：
```
(gdb) b *0x7d61
Breakpoint 3 at 0x7d61
(gdb) c
Continuing.
=> 0x7d61:  call   *0x10018

Breakpoint 3, 0x00007d61 in ?? ()
(gdb) x /16xb 0x100000
0x100000:   0x02    0xb0    0xad    0x1b    0x00    0x00    0x00    0x00
0x100008:   0xfe    0x4f    0x52    0xe4    0x66    0xc7    0x05    0x72
```

