>In the file kern/pmap.c, you must implement code for the following functions.
- pgdir_walk()
- boot_map_region()
- page_lookup()
- page_remove()
- page_insert()
>check_page(), called from mem_init(), tests your page table management routines.

要求：实现以上函数。

# 分析

实现pgdir_walk时注意：
1. pde 中存放的应该是页表的物理地址，pte 中存放的应该是一页的起始物理地址，否则若存放的是虚拟地址，地址转换时会出现循环查找的情况。
2. 最后应该将要返回的页表条目的物理地址转换为虚拟地址，然后再返回，因为后续需要对这个地址进行操作时MMU会自动转换，程序中应该使用虚拟地址。

实现page_lookup时注意：
1. 在处理二级指针pte_t **pte_store 时，应该 *pte_store = pte_ptr，而不能够pte_store = *pte_ptr，否则无法起到修改外部的一级指针的值，外部传入参数为&pte_store。

MMU在地址转换查询 pde 、pte 时都会检查相应的 permision。

page_insert时注意：
1. 由于可能出现重复映射同一物理页到同一页表的同一位置的情况，所以需要注意增加引用值的语句`pp->pp_ref++`应该放在删除该位置上旧物理页的操作之前，否则会出错。

至此如果一切顺利，将通过mem_init()中check_page()的所有assert。


