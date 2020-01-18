>Exercise 2. In the file env.c, finish coding the following functions:

```
env_init()
    Initialize all of the Env structures in the envs array and add them to the env_free_list. Also calls env_init_percpu, which configures the segmentation hardware with separate segments for privilege level 0 (kernel) and privilege level 3 (user).

env_setup_vm()
    Allocate a page directory for a new environment and initialize the kernel portion of the new environment's address space.

region_alloc()
    Allocates and maps physical memory for an environment

load_icode()
    You will need to parse an ELF binary image, much like the boot loader already does, and load its contents into the user address space of a new environment.

env_create()
    Allocate an environment with env_alloc and call load_icode to load an ELF binary into it.

env_run()
    Start a given environment running in user mode.
```

```
As you write these functions, you might find the new cprintf verb %e useful -- it prints a description corresponding to an error code. For example,

	r = -E_NO_MEM;
	panic("env_alloc: %e", r);
will panic with the message "env_alloc: out of memory".
```

要求：完善 env.c 中用以设置用户环境的函数。

# 代码

env_init() 除了构建初始的用户环境结构队列 env_free_list 外，还调用了 env_init_percpu() 来初始化区分了内核段和用户段的 GDT ，这是为了利用 x86 分段机制的段保护功能将内核与用户区分，注意与 boot loader 设置的 GDT 区分。
```
// Mark all environments in 'envs' as free, set their env_ids to 0,
// and insert them into the env_free_list.
// Make sure the environments are in the free list in the same order
// they are in the envs array (i.e., so that the first call to
// env_alloc() returns envs[0]).
//
void
env_init(void)
{
	// Set up envs array
	// LAB 3: Your code here.
	struct Env *p = envs;
	env_free_list = envs;
	struct Env *tail = envs;
	bool first = true;
	for(; p < envs + NENV; p++) {
		p->env_status = ENV_FREE;
		p->env_id = 0;
		if(first) {
			first = false;
		}
		else {
			tail->env_link = p;
			tail = p;
		}
	}
	// Per-CPU part of the initialization
	env_init_percpu();
}

// Load GDT and segment descriptors.
void
env_init_percpu(void)
{
	lgdt(&gdt_pd);
	// The kernel never uses GS or FS, so we leave those set to
	// the user data segment.
	asm volatile("movw %%ax,%%gs" : : "a" (GD_UD|3));
	asm volatile("movw %%ax,%%fs" : : "a" (GD_UD|3));
	// The kernel does use ES, DS, and SS.  We'll change between
	// the kernel and user data segments as needed.
	asm volatile("movw %%ax,%%es" : : "a" (GD_KD));
	asm volatile("movw %%ax,%%ds" : : "a" (GD_KD));
	asm volatile("movw %%ax,%%ss" : : "a" (GD_KD));
	// Load the kernel text segment into CS.
	asm volatile("ljmp %0,$1f\n 1:\n" : : "i" (GD_KT));
	// For good measure, clear the local descriptor table (LDT),
	// since we don't use it.
	lldt(0);
}
```

每个用户环境应该有一个独立的 env_pgdir，映射内核的部分可以直接使用 kern_pgdir 所包含的内核部分，除了 UVPT 部分。
```
// Initialize the kernel virtual memory layout for environment e.
// Allocate a page directory, set e->env_pgdir accordingly,
// and initialize the kernel portion of the new environment's address space.
// Do NOT (yet) map anything into the user portion
// of the environment's virtual address space.
//
// Returns 0 on success, < 0 on error.  Errors include:
//	-E_NO_MEM if page directory or table could not be allocated.
//
static int
env_setup_vm(struct Env *e)
{
	int i;
	struct PageInfo *p = NULL;

	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	// Now, set e->env_pgdir and initialize the page directory.
	//
	// Hint:
	//    - The VA space of all envs is identical above UTOP
	//	(except at UVPT, which we've set below).
	//	See inc/memlayout.h for permissions and layout.
	//	Can you use kern_pgdir as a template?  Hint: Yes.
	//	(Make sure you got the permissions right in Lab 2.)
	//    - The initial VA below UTOP is empty.
	//    - You do not need to make any more calls to page_alloc.
	//    - Note: In general, pp_ref is not maintained for
	//	physical pages mapped only above UTOP, but env_pgdir
	//	is an exception -- you need to increment env_pgdir's
	//	pp_ref for env_free to work correctly.
	//    - The functions in kern/pmap.h are handy.

	// LAB 3: Your code here.
	e->env_pgdir = (pde_t *)page2kva(p);
	p->pp_ref++;
	memcpy(e->env_pgdir, kern_pgdir, PGSIZE);

	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}
```

region_alloc() 需要分配物理页，然后在 env_pgdir 中将其映射。
```
// Allocate len bytes of physical memory for environment env,
// and map it at virtual address va in the environment's address space.
// Does not zero or otherwise initialize the mapped pages in any way.
// Pages should be writable by user and kernel.
// Panic if any allocation attempt fails.
//
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	// LAB 3: Your code here.
	// (But only if you need it for load_icode.)
	//
	// Hint: It is easier to use region_alloc if the caller can pass
	//   'va' and 'len' values that are not page-aligned.
	//   You should round va down, and round (va + len) up.
	//   (Watch out for corner-cases!)
	uintptr_t start = (uintptr_t)ROUNDDOWN(va, PGSIZE);
	uintptr_t end = (uintptr_t)ROUNDUP(va + len, PGSIZE);
	size_t size = (size_t)(end - start);
	size_t pgs = size / PGSIZE;
	if (size % PGSIZE != 0) {
			pgs++;
	}
	int r;
	for(int i = 0; i < pgs; i++) {
		struct PageInfo *pp;
		if((pp = page_alloc(0)) == NULL) {
			panic("page_alloc failed at region_alloc for env %d.\n", e->env_id);
		}
		if((r = page_insert(e->env_pgdir, pp, (void *)(start + i*PGSIZE), PTE_W | PTE_U)) < 0) {
			panic("page_insert failed at region_alloc for env %d : %e.\n", e->env_id, r);
		}
	}
}
```

load_icode() 将二进制 ELF 格式文件中的段读入地址空间，设置 tf 的 eip，分配一页空间给用户栈。
```
static void
load_icode(struct Env *e, uint8_t *binary)
{
	// Hints:
	//  Load each program segment into virtual memory
	//  at the address specified in the ELF segment header.
	//  You should only load segments with ph->p_type == ELF_PROG_LOAD.
	//  Each segment's virtual address can be found in ph->p_va
	//  and its size in memory can be found in ph->p_memsz.
	//  The ph->p_filesz bytes from the ELF binary, starting at
	//  'binary + ph->p_offset', should be copied to virtual address
	//  ph->p_va.  Any remaining memory bytes should be cleared to zero.
	//  (The ELF header should have ph->p_filesz <= ph->p_memsz.)
	//  Use functions from the previous lab to allocate and map pages.
	//
	//  All page protection bits should be user read/write for now.
	//  ELF segments are not necessarily page-aligned, but you can
	//  assume for this function that no two segments will touch
	//  the same virtual page.
	//
	//  You may find a function like region_alloc useful.
	//
	//  Loading the segments is much simpler if you can move data
	//  directly into the virtual addresses stored in the ELF binary.
	//  So which page directory should be in force during
	//  this function?
	//
	//  You must also do something with the program's entry point,
	//  to make sure that the environment starts executing there.
	//  What?  (See env_run() and env_pop_tf() below.)

	// LAB 3: Your code here.
	struct Proghdr *ph, *eph;
	struct Elf *elfhdr = (struct Elf *)binary;
	if (elfhdr->e_magic != ELF_MAGIC)
		panic("not elf\n");

	ph = (struct Proghdr *) ((uint8_t *) elfhdr + elfhdr->e_phoff);
	eph = ph + elfhdr->e_phnum;

	lcr3(PADDR(e->env_pgdir));   // for the memcpy later

	for (; ph < eph; ph++) {
		// p_pa is the load address of this segment (as well
		// as the physical address)
		if(ph->p_type == ELF_PROG_LOAD) {
			region_alloc(e, (void *)ph->p_va, ph->p_memsz);
			memset((void *)ph->p_va, 0, ph->p_memsz);
			memcpy((void *)ph->p_va, (void *)binary + ph->p_offset, ph->p_filesz);
		}
	}

	lcr3(PADDR(kern_pgdir));
	e->env_tf.tf_eip = elfhdr->e_entry;

	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.
	// LAB 3: Your code here.
	region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);
}
```

在 env_create() 中调用了 env_alloc() 来分配环境结构，此处总是分配 env_free_list 中的第一个结构。
```
// Allocates a new env with env_alloc, loads the named elf
// binary into it with load_icode, and sets its env_type.
// This function is ONLY called during kernel initialization,
// before running the first user-mode environment.
// The new env's parent ID is set to 0.
//
void
env_create(uint8_t *binary, enum EnvType type)
{
	// LAB 3: Your code here.
	int r;
	struct Env *newenv;
	if((r = env_alloc(&newenv, (envid_t)0)) < 0)
		panic("env_alloc failed: %e.\n", r);
	load_icode(newenv, binary);
	newenv->env_type = type;
}
```

```
// Context switch from curenv to env e.
// Note: if this is the first call to env_run, curenv is NULL.
//
// This function does not return.
//
void
env_run(struct Env *e)
{
	// LAB 3: Your code here.
	//panic("env_run not yet implemented");
	if(curenv && curenv->env_status == ENV_RUNNING)
		curenv->env_status = ENV_RUNNABLE;
	curenv = e;
	curenv->env_status = ENV_RUNNING;
	curenv->env_runs++;
	lcr3(PADDR(e->env_pgdir));

	env_pop_tf(&e->env_tf);
}

```