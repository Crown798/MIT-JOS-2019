>Exercise 5. Fill in the missing code in mem_init() after the call to check_page().

>Your code should now pass the check_kern_pgdir() and check_page_installed_pgdir() checks.

要求：完善mem_init()中初始化内核页目录中的内核部分空间。

# 分析

只需设置 RO PAGES 、CPU0's Kernel Stack 、KERNBASE以上这三部分即可。

Cur. Page Table 部分在mem_init()开始时即被设置，`kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;`

注意：
1. 'bootstack'定义在/kernel/entry
2. 未设置perm的页表条目即权限为不可访问，所以只需设置内核或用户需要访问的条目即可
3. 页表条目设置为PTE_P后即为可读
4. 一个PTSIZE是一个页表映射的所有物理页大小，即4MB

至此，编译启动后，通过check_kern_pgdir() and check_page_installed_pgdir()的测试。
