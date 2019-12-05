>Exercise 3. Take a look at the lab tools guide, especially the section on GDB commands. Even if you're familiar with GDB, this includes some esoteric GDB commands that are useful for OS work.

>Set a breakpoint at address 0x7c00, which is where the boot sector will be loaded. Continue execution until that breakpoint. Trace through the code in boot/boot.S, using the source code and the disassembly file obj/boot/boot.asm to keep track of where you are. Also use the x/i command in GDB to disassemble sequences of instructions in the boot loader, and compare the original boot loader source code with both the disassembly in obj/boot/boot.asm and GDB.

>Trace into bootmain() in boot/main.c, and then into readsect(). Identify the exact assembly instructions that correspond to each of the statements in readsect(). Trace through the rest of readsect() and back out into bootmain(), and identify the begin and end of the for loop that reads the remaining sectors of the kernel from the disk. Find out what code will run when the loop is finished, set a breakpoint there, and continue to that breakpoint. Then step through the remainder of the boot loader.

# 1 关注GDB中对调试OS有用的一些指令
Ctrl-c：
Halt the machine and break in to GDB at the currentinstruction. If QEMU has multiple virtual CPUs, this halts all of them.

c (or continue)：
Continue execution until the next breakpoint or Ctrl-c.

si (or stepi)：
Execute one machine instruction.

b function or b file:line (or breakpoint)：
Set a breakpoint at the given function or line.

b *addr (or breakpoint)：
Set a breakpoint at the EIP addr.

set print pretty：
Enable pretty-printing of arrays and structs.

info registers：
Print the general purpose registers, eip, eflags, and the segment selectors. For a much more thorough dump of the machine register state, see QEMU's own info registers command.

x/Nx addr：
Display a hex dump of N words starting at virtual address addr. If N is omitted, it defaults to 1. addr can be any expression.

x/Ni addr：
Display the N assembly instructions starting at addr. Using $eip as addr will display the instructions at the current instruction pointer.

symbol-file file：
(Lab 3+) Switch to symbol file file. When GDB attaches to QEMU, it has no notion of the process boundaries within the virtual machine, so we have to tell it which symbols to use. By default, we configure GDB to use the kernel symbol file, obj/kern/kernel. If the machine is running user code, say hello.c, you can switch to the hello symbol file using symbol-file obj/user/hello.

QEMU represents each virtual CPU as a thread in GDB, so you can use all of GDB's thread-related commands to view or manipulate QEMU's virtual CPUs.

thread n：
GDB focuses on one thread (i.e., CPU) at a time. This command switches that focus to thread n, numbered from zero.

info threads：
List all threads (i.e., CPUs), including their state (active or halted) and what function they're in.

# 2 在boot loader开始执行处0x7c00设置断点，跟踪遍历boot/boot.S的汇编代码，比较GDB执行的汇编语句、obj/boot/boot.asm中的语句、源文件boot/boot.S中的语句
.S文件中的汇编代码，在经过编译链接后会形成机器（GDB）实际执行的机器语句，反汇编后即形成.asm文件中的汇编语句。

所以.asm文件有助于调试、了解机器实际的执行过程。

注意：
.asm文件中会包含.S中的变量数据的反汇编形式，形成了非法指令，如：
```
gdt:
  SEG_NULL              # null seg
  SEG(STA_X|STA_R, 0x0, 0xffffffff) # code seg
  SEG(STA_W, 0x0, 0xffffffff)           # data seg

gdtdesc:
  .word   0x17                            # sizeof(gdt) - 1
  .long   gdt                             # address gdt
```
会被反汇编为：
```
00007c4c <gdt>:
	...
    7c54:	ff                   	(bad)  
    7c55:	ff 00                	incl   (%eax)
    7c57:	00 00                	add    %al,(%eax)
    7c59:	9a cf 00 ff ff 00 00 	lcall  $0x0,$0xffff00cf
    7c60:	00                   	.byte 0x0
    7c61:	92                   	xchg   %eax,%edx
    7c62:	cf                   	iret   
	...

00007c64 <gdtdesc>:
    7c64:	17                   	pop    %ss
    7c65:	00 4c 7c 00          	add    %cl,0x0(%esp,%edi,2)
	...
```

# 3 跟踪遍历boot/main.c中的bootmain()函数，以及bootmain()调用 readsect()的过程