# Learning Jos From Scratch: Lab3

## Overview

This lab is mainly about **User Environments**.

## Exercise 

### Exercise 1

#### Question

Modify `mem_init()` in `kern/pmap.c` to allocate and map the `envs` array. This array consists of exactly `NENV` instances of the `Env` structure allocated much like how you allocated the `pages` array. Also like the `pages` array, the memory backing `envs` should also be mapped user read-only at `UENVS` (defined in `inc/memlayout.h`) so user processes can read from this array.

You should run your code and make sure `check_kern_pgdir()` succeeds.

#### Answer

Add extra code in `mem_init()` function

```c++
//////////////////////////////////////////////////////////////////////
// Make 'envs' point to an array of size 'NENV' of 'struct Env'.
n = ROUNDUP(NENV*sizeof(struct Env), PGSIZE);
envs = (struct Env *) boot_alloc(n);
memset(envs, 0, n);

// Map the 'envs' array read-only by the user at linear address UENVS
// (ie. perm = PTE_U | PTE_P).
// Permissions:
//    - the new image at UENVS  -- kernel R, user R
//    - envs itself -- kernel RW, user NONE
boot_map_region(kern_pgdir, UENVS, n, PADDR(envs), PTE_U);
```



### Exercise 2

#### Question

In the file `env.c`, finish coding the following functions:

- `env_init()`

  Initialize all of the `Env` structures in the `envs` array and add them to the `env_free_list`. Also calls `env_init_percpu`, which configures the segmentation hardware with separate segments for privilege level 0 (kernel) and privilege level 3 (user).

- `env_setup_vm()`

  Allocate a page directory for a new environment and initialize the kernel portion of the new environment's address space.

- `region_alloc()`

  Allocates and maps physical memory for an environment

- `load_icode()`

  You will need to parse an ELF binary image, much like the boot loader already does, and load its contents into the user address space of a new environment.

- `env_create()`

  Allocate an environment with `env_alloc` and call `load_icode` to load an ELF binary into it.

- `env_run()`

  Start a given environment running in user mode.

As you write these functions, you might find the new cprintf verb `%e` useful -- it prints a description corresponding to an error code. For example,

```c++
r = -E_NO_MEM;
panic("env_alloc: %e", r);
```

will panic with the message "env_alloc: out of memory".

#### Answer

Look at the book (though chapter 5 is enough)

### Exercise 3

#### Question

*While GDB can only access QEMU's memory by virtual address, it's often useful to be able to inspect physical memory while setting up virtual memory. Review the QEMU [monitor commands](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/labguide.html#qemu) from the lab tools guide, especially the `xp` command, which lets you inspect physical memory. To access the QEMU monitor, press Ctrl-a c in the terminal (the same binding returns to the serial console)*.

*Use the xp command in the QEMU monitor and the x command in GDB to inspect memory at corresponding physical and virtual addresses and make sure you see the same data.*

*Our patched version of QEMU provides an info pg command that may also prove useful: it shows a compact but detailed representation of the current page tables, including all mapped memory ranges, permissions, and flags. Stock QEMU also provides an info mem command that shows an overview of which ranges of virtual addresses are mapped and with what permissions.*

#### Answer

Make sure you use the **pathched QEMU**

```shell
(gdb) x/16xw 0xf0100000
0xf0100000: 	0x1badb002      0x00000000      0xe4524ffe      0x7205c766
0xf0100010:   0x34000004      0x6000b812      0x220f0011      0xc0200fd8
0xf0100020:  	0x0100010d      0xc0220f80      0x10002fb8      0xbde0fff0
0xf0100030:   0x00000000      0x116000bc      0x0002e8f0      0xfeeb0000

(qemu) xp/16xw 0x00100000
0000000000100000: 0x1badb002 0x00000000 0xe4524ffe 0x7205c766
0000000000100010: 0x34000004 0x6000b812 0x220f0011 0xc0200fd8
0000000000100020: 0x0100010d 0xc0220f80 0x10002fb8 0xbde0fff0
0000000000100030: 0x00000000 0x116000bc 0x0002e8f0 0xfeeb0000
```

#### Question

*Assuming that the following JOS kernel code is correct, what type should variable `x` have `uintptr_t` or `physaddr_t`?*

```c++
mystery_t x;
char* value = return_a_pointer();
*value = 10;
x = (mystery_t) value;
```

#### Answer

Because we dereference `value` , so `value` must be virtual address. 

### Exercise 4

#### Question

*In the file `kern/pmap.c`, you must implement code for the following functions.*

```c
pgdir_walk()
boot_map_region()
page_lookup()
page_remove()
page_insert()
```

*`check_page()`, called from `mem_init()`, tests your page table management routines. You should make sure it reports success before proceeding.*

#### Answer

```c
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	if((pgdir[PDX(va)] & PTE_P) == 0) {
		if (create == false) {
			return NULL;
		} else {
			struct PageInfo* new_page = page_alloc(ALLOC_ZERO);
			if (new_page == NULL) {
				return NULL;
			} else {
				pgdir[PDX(va)] = PTE_ADDR(page2pa(new_page)) | PTE_P | PTE_W;
				new_page -> pp_ref += 1;
			}
		}
	}
	return (pte_t *)(KADDR(PTE_ADDR(pgdir[PDX(va)]))) + PTX(va);
}

static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	size_t n = ROUNDUP(size, PGSIZE);
	for (int i = 0; i < n; i += PGSIZE) {
		pte_t *pte = pgdir_walk(pgdir, (void *)(va + i), 1);
		if (pte == NULL) {
			return;
		}
		*pte = PTE_ADDR(pa + i) | perm | PTE_P;
	}

}

struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	pte_t *pte = pgdir_walk(pgdir, va, 0);
	if (pte == NULL) {
		return NULL;
	}
	if ((*pte & PTE_P) == 0) {
		return NULL;
	} else {
		if (pte_store) {
			*pte_store = pte;
		}
		return pa2page(PTE_ADDR(*pte));
	}
	return NULL;
}

void
page_remove(pde_t *pgdir, void *va)
{
	// Page Table Entry
	pte_t *pte;
	struct PageInfo* page = page_lookup(pgdir, va, &pte);
	if (!page) {
		return;
	}
	// If already mapped
	if ((*pte & PTE_P) == 1) {
		// Decrease pp -> pp_ref
		page_decref(page);
		// Invalidate
		tlb_invalidate(pgdir, va);
		// Clera Page Table Entry
		*pte = 0;
	} 
	return;
}

int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{	
	// Page Table Entry
	pte_t *pte = pgdir_walk(pgdir, va, 1);
	if (pte == NULL) {
		return -E_NO_MEM;
	}
	if ((*pte & PTE_P) == 1) {
		if (pa2page(PTE_ADDR(*pte))!= pp) {
			page_remove(pgdir, va);
			pp -> pp_ref += 1;
		}
	} else {
		pp -> pp_ref += 1;
	}
	*pte = PTE_ADDR(page2pa(pp)) | perm | PTE_P;
	pgdir[PDX(va)] |= perm;
	return 0;
}

```

### Exercise 5

#### Question

*Fill in the missing code in `mem_init()` after the call to `check_page()`.*

#### Answer

```c
boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U)
boot_map_region(kern_pgdir, KSTACKTOP - KSTKSIZE, KSTKSIZE, PADDR(bootstack), PTE_W);
boot_map_region(kern_pgdir, KERNBASE, 1 << 28, 0, PTE_W);
```

#### Question

*We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?*

#### Answer

Using bit `PTE_W` and `PTE_U`. 

#### Question

*What is the maximum amount of physical memory that this operating system can support? Why?*

#### Answer

Since JOS use at most 4MB `Pageinfo` to manage memory, each struct is 8B. So we can at most support 512k pages. And the total physical memory is 2GB. 

#### Question

How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

#### Answer

If we have 2G physical memory:

- 4MB for  `Pageinfo`
- 2MB for page table
- 4KB for page directory



