>Exercise 1. In the file kern/pmap.c, you must implement code for the following functions (probably in the order given).
- boot_alloc()
- mem_init() (only up to the call to check_page_free_list(1))
- page_init()
- page_alloc()
- page_free()
>check_page_free_list() and check_page_alloc() test your physical page allocator. 

要求：完善 kern/pmap.c 中的函数，实现物理内存分配器。

# 分析

mem_init() 物理内存初始化的部分流程：
1. 进入内核后首先调用的是i386_init()，该函数会调用mem_init()
2. mem_init()首先调用i386_detect_memory()来计算有多少可用的物理内存页保存到npages和npages_basemem中
3. 调用boot_alloc(PGSIZE)给kern_pgdir 分配一页空间，并进行相应设置
4. boot_alloc(npages * sizeof(struct PageInfo)) 给物理页管理表数组 pages 分配相应空间
5. page_init() 初始化物理页已分配、未分配情况
6. check_page_free_list(1)、check_page_alloc()、check_page() 检查

实现page_init时注意，已经使用的物理空间从下到上为：
- unused
- pages (npages * sizeof(struct PageInfo))
- kern_pgdir (PGSIZE)
- kern_data_and_text
- IO hole
- unused (including aborted kernel_elf and boot_code)
- real-mode IDT and BIOS structures (PGSIZE)

至此编译启动后，check_page_free_list() and check_page_alloc() 会输出测试成功信息。