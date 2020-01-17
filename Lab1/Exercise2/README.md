>Exercise 2. Use GDB's si (Step Instruction) command to trace into the ROM BIOS for a few more instructions, and try to guess what it might be doing. No need to figure out all the details - just the general idea of what the BIOS is doing first.

要求：使用GDB的si指令来跟踪BIOS的前几条指令，弄清楚BIOS一开始运行时做了什么。

# 分析
进入GDB：
```
GNU gdb (GDB) 6.8-debian
Copyright (C) 2008 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "i486-linux-gnu".
+ target remote localhost:26000
The target architecture is assumed to be i8086

[f000:fff0] 0xffff0:    ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb) 
```
可见BIOS的第一条指令在0xf000:0xfff0处，该条指令为`ljmp $0xf000,$0xe05b`。

输入info r可以看到当前的寄存器情况：
```
(gdb) info r
eip            0xfff0	0xfff0
eflags         0x2	[ ]
cs             0xf000	61440
```
输入x /10i 0xffff0可以查看0xffff0处的指令：
```
(gdb) x /10i 0xffff0
   0xffff0:	ljmp   $0xf000,$0xe05b
   0xffff5:	xor    %dh,0x322f
   0xffff9:	xor    (%bx),%bp
   0xffffb:	cmp    %di,(%bx,%di)
   0xffffd:	add    %bh,%ah
   0xfffff:	add    %al,(%bx,%si)
   0x100001:	add    %al,(%bx,%si)
   0x100003:	add    %al,(%bx,%si)
   0x100005:	add    %al,(%bx,%si)
   0x100007:	add    %al,(%bx,%si)
```
第一条指令是一个长跳转指令，跳转到0xfe05b处执行，继续看0xfe05b处的指令：
```
(gdb) x /100i 0xfe05b
   0xfe05b:	cmpl   $0x0,%cs:0x65a4
   0xfe062:	jne    0xfd2b9
   0xfe066:	xor    %ax,%ax
   0xfe068:	mov    %ax,%ss
   0xfe06a:	mov    $0x7000,%esp
   0xfe070:	mov    $0xf3c4d,%edx
   0xfe076:	jmp    0xfd12a
   0xfe079:	push   %ebp
   0xfe07b:	push   %edi
   0xfe07d:	push   %esi
   0xfe07f:	push   %ebx
   0xfe081:	sub    $0xc,%esp
   0xfe085:	mov    %eax,%esi
   0xfe088:	mov    %edx,(%esp)
   0xfe08d:	mov    %ecx,%edi
   0xfe090:	mov    $0xe000,%eax
   0xfe096:	mov    %ax,%es
   0xfe098:	mov    %es:0xf7a0,%al
   0xfe09c:	mov    %al,0xa(%esp)
   0xfe0a1:	and    $0xffffffcc,%eax
   0xfe0a5:	or     $0x30,%eax
   0xfe0a9:	mov    %al,0xb(%esp)
   0xfe0ae:	lea    0xb(%esp),%edx
   0xfe0b4:	mov    $0x1060,%eax
   0xfe0ba:	calll  0xf8431
   0xfe0c0:	test   %eax,%eax
   0xfe0c3:	jne    0xfe2ad
   0xfe0c7:	calll  0xf6ffc
   0xfe0cd:	test   %esi,%esi
   0xfe0d0:	je     0xfe0da
   0xfe0d2:	andb   $0xdf,0xb(%esp)
   0xfe0d8:	jmp    0xfe0e0
   0xfe0da:	andb   $0xef,0xb(%esp)
   0xfe0e0:	lea    0xb(%esp),%edx
   0xfe0e6:	mov    $0x1060,%eax
   0xfe0ec:	calll  0xf8431
   0xfe0f2:	mov    %eax,%ebx
   0xfe0f5:	test   %eax,%eax
   0xfe0f8:	jne    0xfe296
   0xfe0fc:	cmpl   $0x2ff,(%esp)
   0xfe105:	jne    0xfe164
   0xfe107:	mov    $0x3e8,%ecx
   0xfe10d:	mov    $0xff,%edx
   0xfe113:	mov    %esi,%eax
   0xfe116:	calll  0xf8ed0
   0xfe11c:	mov    %eax,%ebx
   0xfe11f:	test   %eax,%eax
   0xfe122:	jne    0xfe296
   0xfe126:	mov    $0xfa0,%ecx
   0xfe12c:	xor    %edx,%edx
   0xfe12f:	mov    %esi,%eax
   0xfe132:	calll  0xf8df9
   0xfe138:	test   %eax,%eax
   0xfe13b:	js     0xfe293
   0xfe13f:	mov    %al,(%edi)
   0xfe142:	mov    $0x64,%ecx
   0xfe148:	xor    %edx,%edx
   0xfe14b:	mov    %esi,%eax
   0xfe14e:	calll  0xf8df9
   0xfe154:	mov    %eax,%edx
   0xfe157:	not    %edx
   0xfe15a:	sar    $0x1f,%edx
   0xfe15e:	and    %edx,%eax
   0xfe161:	jmp    0xfe1eb
   0xfe164:	cmpl   $0x2f2,(%esp)
   0xfe16d:	jne    0xfe1fa
   0xfe171:	mov    $0xc8,%ecx
   0xfe177:	mov    $0xf2,%edx
   0xfe17d:	mov    %esi,%eax
   0xfe180:	calll  0xf8ed0
   0xfe186:	mov    %eax,%ebx
   0xfe189:	test   %eax,%eax
   0xfe18c:	jne    0xfe296
   0xfe190:	mov    $0x1f4,%ecx
   0xfe196:	xor    %edx,%edx
   0xfe199:	mov    %esi,%eax
   0xfe19c:	calll  0xf8df9
   0xfe1a2:	test   %eax,%eax
   0xfe1a5:	js     0xfe293
   0xfe1a9:	mov    %al,(%edi)
   0xfe1ac:	lea    -0xab(%eax),%edx
   0xfe1b4:	cmp    $0x1,%edx
   0xfe1b8:	jbe    0xfe1d2
   0xfe1ba:	cmp    $0x2b,%eax
   0xfe1be:	je     0xfe1d2
   0xfe1c0:	cmp    $0x5d,%eax
   0xfe1c4:	je     0xfe1d2
   0xfe1c6:	cmp    $0x60,%eax
   0xfe1ca:	je     0xfe1d2
   0xfe1cc:	cmp    $0x47,%eax
   0xfe1d0:	jne    0xfe1f2
   0xfe1d2:	mov    $0x1f4,%ecx
   0xfe1d8:	xor    %edx,%edx
   0xfe1db:	mov    %esi,%eax
   0xfe1de:	calll  0xf8df9
   0xfe1e4:	test   %eax,%eax
   0xfe1e7:	js     0xfe293
   0xfe1eb:	mov    %al,0x1(%edi)
   0xfe1ef:	jmp    0xfe296
   0xfe1f2:	movb   $0x0,0x1(%edi)
```
