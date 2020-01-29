>Exercise 2. Read boot_aps() and mp_main() in kern/init.c , and the assembly code in kern/mpentry.S . Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of page_init() in kern/pmap.c to avoid adding the page at MPENTRY_PADDR to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated check_page_free_list() test (but might fail the updated check_kern_pgdir() test, which we will fix soon).

# 代码

```
void
page_init(void)
{
	// LAB 4:
	// Change your code to mark the physical page at MPENTRY_PADDR
	// as in use

	// The example code here marks all physical pages as free.
	// However this is not truly the case.  What memory is free?
	//  1) Mark physical page 0 as in use.
	//     This way we preserve the real-mode IDT and BIOS structures
	//     in case we ever need them.  (Currently we don't, but...)
	//  2) The rest of base memory, [PGSIZE, npages_basemem * PGSIZE)
	//     is free.
	//  3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM), which must
	//     never be allocated.
	//  4) Then extended memory [EXTPHYSMEM, ...).
	//     Some of it is in use, some is free. Where is the kernel
	//     in physical memory?  Which pages are already in use for
	//     page tables and other data structures?
	//
	// Change the code to reflect this.
	// NB: DO NOT actually touch the physical memory corresponding to
	// free pages!
	size_t i;
	size_t io_hole_start_page = (size_t) IOPHYSMEM / PGSIZE;
	size_t kernel_used_page = (size_t) PADDR(boot_alloc(0)) / PGSIZE;
	size_t mpentry_used_page = (size_t) MPENTRY_PADDR / PGSIZE;
	for (i = 0; i < npages; i++) {
		if(i == 0) {
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
		}
		else if(i == mpentry_used_page) {
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
		}
		else if(i >= io_hole_start_page && i < kernel_used_page) {
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
		}
		else {
			pages[i].pp_ref = 0;
			pages[i].pp_link = page_free_list;
			page_free_list = &pages[i];
		}
	}
}
```