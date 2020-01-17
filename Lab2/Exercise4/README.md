>In the file kern/pmap.c, you must implement code for the following functions.
- pgdir_walk()
- boot_map_region()
- page_lookup()
- page_remove()
- page_insert()
>check_page(), called from mem_init(), tests your page table management routines.

要求：实现以上函数。

# 分析

MMU在地址转换查询 pde 、pte 时都会检查相应的 permision。

至此如果一切顺利，将通过mem_init()中check_page()的所有assert。

# 代码

实现pgdir_walk时注意：
1. pde 中存放的应该是页表的物理地址，pte 中存放的应该是一页的起始物理地址，否则若存放的是虚拟地址，地址转换时会出现循环查找的情况。
2. 最后应该将要返回的页表条目的物理地址转换为虚拟地址，然后再返回，因为后续需要对这个地址进行操作时MMU会自动转换，程序中应该使用虚拟地址。
```
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	// Fill this function in
	pde_t* pde_ptr = pgdir + PDX(va);
  if (!(*pde_ptr & PTE_P)) {                              //页表还没有分配
      if (create) {
          //分配一个页作为页表
          struct PageInfo *pp = page_alloc(1);
          if (pp == NULL) {
              return NULL;
          }
          pp->pp_ref++;
          *pde_ptr = (page2pa(pp)) | PTE_P | PTE_U | PTE_W;   //更新页目录项
      } else {
          return NULL;
      }
  }

  return (pte_t *)KADDR(PTE_ADDR(*pde_ptr)) + PTX(va);
}
```

```
// Map [va, va+size) of virtual address space to physical [pa, pa+size)
// in the page table rooted at pgdir.  Size is a multiple of PGSIZE, and
// va and pa are both page-aligned.
// Use permission bits perm|PTE_P for the entries.
//
// This function is only intended to set up the ``static'' mappings
// above UTOP. As such, it should *not* change the pp_ref field on the
// mapped pages.
//
// Hint: the TA solution uses pgdir_walk
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// Fill this function in
	size_t pgs = size / PGSIZE;
	if (size % PGSIZE != 0) {
			pgs++;
	}                            //计算总共有多少页
	for (int i = 0; i < pgs; i++) {
			pte_t *pte = pgdir_walk(pgdir, (void *)va, 1);//获取va对应的PTE的地址
			if (pte == NULL) {
					panic("boot_map_region(): out of memory\n");
			}
			*pte = pa | PTE_P | perm; //修改va对应的PTE的值
			pa += PGSIZE;             //更新pa和va，进行下一轮循环
			va += PGSIZE;
	}
}
```

实现page_lookup时注意：
1. 在处理二级指针pte_t **pte_store 时，应该 *pte_store = pte_ptr，而不能够pte_store = *pte_ptr，否则无法起到修改外部的一级指针的值，外部传入参数为&pte_store。
```
// Return the page mapped at virtual address 'va'.
// If pte_store is not zero, then we store in it the address
// of the pte for this page.  This is used by page_remove and
// can be used to verify page permissions for syscall arguments,
// but should not be used by most callers.
//
// Return NULL if there is no page mapped at va.
//
// Hint: the TA solution uses pgdir_walk and pa2page.
//
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	// Fill this function in
  pte_t *pte =  pgdir_walk(pgdir, va, 0);         //如果对应的页表不存在，不进行创建
  if (pte == NULL) {
      return NULL;
  }
  if (!((*pte) & PTE_P)) {
      return NULL;
  }
  physaddr_t pa = PTE_ADDR(*pte);                 //va对应的物理
  struct PageInfo *pp = pa2page(pa);                               //物理地址对应的PageInfo结构地址
  if (pte_store != NULL) {
      *pte_store = pte;
  }
  return pp;
}
```

```
void
page_remove(pde_t *pgdir, void *va)
{
	// Fill this function in
	pte_t *pte_store;
  struct PageInfo *pp = page_lookup(pgdir, va, &pte_store); //获取va对应的PTE的地址以及pp结构
  if (pp == NULL) {    //va可能还没有映射，那就什么都不用做
      return;
  }
  page_decref(pp);    //将pp->pp_ref减1，如果pp->pp_ref为0，需要释放该PageInfo结构（将其放入page_free_list链表中）
  *pte_store = 0;    //将PTE清空
  tlb_invalidate(pgdir, va); //失效化TLB缓存
}
```

page_insert时注意：
1. 由于可能出现重复映射同一物理页到同一页表的同一位置的情况，所以需要注意增加引用值的语句`pp->pp_ref++`应该放在删除该位置上旧物理页的操作之前，否则会出错。
```
// Map the physical page 'pp' at virtual address 'va'.
// The permissions (the low 12 bits) of the page table entry
// should be set to 'perm|PTE_P'.
//
// Requirements
//   - If there is already a page mapped at 'va', it should be page_remove()d.
//   - If necessary, on demand, a page table should be allocated and inserted
//     into 'pgdir'.
//   - pp->pp_ref should be incremented if the insertion succeeds.
//   - The TLB must be invalidated if a page was formerly present at 'va'.
//
// Corner-case hint: Make sure to consider what happens when the same
// pp is re-inserted at the same virtual address in the same pgdir.
// However, try not to distinguish this case in your code, as this
// frequently leads to subtle bugs; there's an elegant way to handle
// everything in one code path.
//
// RETURNS:
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	// Fill this function in
	pte_t *pte = pgdir_walk(pgdir, va, 1);    //拿到va对应的PTE地址，如果va对应的页表还没有分配，则分配一个物理页作为页表
  if (pte == NULL) {
      return -E_NO_MEM;
  }
	pp->pp_ref++;                                       //引用加1 / pay attention to the location!!!
  if ((*pte) & PTE_P) {                               //当前虚拟地址va已经被映射过，需要先释放
      page_remove(pgdir, va);
  }
  physaddr_t pa = page2pa(pp); //将PageInfo结构转换为对应物理页的首地址
  *pte = pa | perm | PTE_P;    //修改PTE

  return 0;
}
```
