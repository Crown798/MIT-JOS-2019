>Exercise 7. Use QEMU and GDB to trace into the JOS kernel and stop at the movl %eax, %cr0. Examine memory at 0x00100000 and at 0xf0100000. Now, single step over that instruction using the stepi GDB command. Again, examine memory at 0x00100000 and at 0xf0100000. Make sure you understand what just happened.

>What is the first instruction after the new mapping is established that would fail to work properly if the mapping weren't in place? Comment out the movl %eax, %cr0 in kern/entry.S, trace into it, and see if you were right.

要求：使用 GDB 验证开启分页功能的语句。

# 结论
语句`movl    %eax, %cr0`的作用就是开启分页模式，在执行这条语句之前，所有的线性地址直接等于物理地址，执行之后，线性地址需要经过MMU的映射才对应到物理地址。

所以执行之前0x100000处为内核代码，0xf0100000处为空。

执行之后0x100000和0xf0100000都映射到物理内存0x100000，所以他们的内容相同。

如果不执行这条语句的话，第一条出错的语句将是后面的绝对跳转语句(kern/entry.S 59)：
```
# Turn on paging.
movl    %cr0, %eax
orl $(CR0_PE|CR0_PG|CR0_WP), %eax
movl    %eax, %cr0

# Now paging is enabled, but we're still running at a low EIP
# (why is this okay?).  Jump up above KERNBASE before entering
# C code.
mov $relocated, %eax
jmp *%eax
```
即第一条出错的语句是`jmp *%eax`，程序将跳到一个错误的地址，该地址内容为空。