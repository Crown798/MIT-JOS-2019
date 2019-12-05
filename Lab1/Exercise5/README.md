>Exercise 5. Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in boot/Makefrag to something wrong, run make clean, recompile the lab with make, and trace into the boot loader again to see what happens. Don't forget to change the link address back and make clean again afterward!

如果将boot loader的link address设置错误，第一条崩溃的指令是哪条？使用GDB跟踪验证。

修改boot/Makefrag中的boot loader的link address（0x7c00改为0x7c10）。
# 结论
第一条出问题的语句是:
```
lgdt gdtdesc
```
第一条直接出错的语句是:
```
ljmp    $PROT_MODE_CSEG, $protcseg
```
# 分析
bootloader被加载到0x7c00处，但连接时指定的地址是0x7c10，那么所有的标号都会错乱。

但是短跳转指令因为使用的是相对地址，所以并不会出问题。

第一条直接使用绝对地址的指令是lgdt gdtdesc，但是这条指令只是加载GDTR寄存器，虽然这个寄存器的内容是有问题的，但是由于还没有访存，所以还没有直接导致出错，后面的ljmp需要访存了，立马就出错了。
