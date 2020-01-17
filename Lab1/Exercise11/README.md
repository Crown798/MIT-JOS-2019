>Exercise 11. Implement the backtrace function as specified above.

要求：补全回溯函数mon_backtrace()，输出格式如下：
```
Stack backtrace:
  ebp f0109e58  eip f0100a62  args 00000001 f0109e80 f0109e98 f0100ed2 00000031
  ebp f0109ed8  eip f01000d6  args 00000000 00000000 f0100058 f0109f28 00000061
  ...
```
/kern/init.c中test_backtrace(5)函数，会测试mon_backtrace()：
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
```
# 分析
由之前的知识，我们知道栈帧中包含了需要输出的参数：ebp、eip、args，并且可以通过ebp可以在栈帧链中转换，获得栈中每个函数的参数。

图4

实验提供了内联汇编实现的read_ebp()函数，可以让我们方便获取寄存器ebp的值。inc/x86.h中：
```
static inline uint32_t
read_ebp(void)
{
	uint32_t ebp;
	asm volatile("movl %%ebp,%0" : "=r" (ebp));
	return ebp;
}
```
# 实现
```
int 
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    // Your code here.
    uint32_t *ebp = (uint32_t *)read_ebp(); //获取ebp的值
    while (ebp != 0) {                      //终止条件是ebp为0
        //打印ebp, eip, 最近的五个参数
        uint32_t eip = *(ebp + 1);
        cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, eip, *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5), *(ebp + 6));
        //更新ebp
        ebp = (uint32_t *)(*ebp);
    }
    return 0;
}
```