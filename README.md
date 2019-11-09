# Learning Jos From Scratch: Lab2

## Overview

This lab is mainly about **memory management**.

## Exercise 

### Exercise 1

#### Question

*In the file `kern/pmap.c`, you must implement code for the following functions (probably in the order given).*

```
boot_alloc()
mem_init() (only up to the call to check_page_free_list(1))
page_init()
page_alloc()
page_free()
```

*`check_page_free_list()` and `check_page_alloc()` test your physical page allocator. You should boot JOS and see whether `check_page_alloc()` reports success. Fix your code so that it passes. You may find it helpful to add your own `assert()`s to verify that your assumptions are correct.*

#### Answer

In `boot_alloc(n)` function, we allocate `n` bytes by increase and roundup `nextfree` pointer. 

```c
static void *
boot_alloc(uint32_t n)
{
	--- Omit some code ---
	// Allocate a chunk large enough to hold 'n' bytes, then update
	// nextfree.  Make sure nextfree is kept aligned
	// to a multiple of PGSIZE.
	//
	// LAB 2: Your code here.
	result = nextfree;
	// Update nextfree
	nextfree = ROUNDUP((char *)(nextfree + n), PGSIZE);
	return result;
}
```

In `mem_init()` function, we use `boot_alloc` function to allocation an array of `npages`. 

```c
void
mem_init(void)
{
  --- Omit some code ---
	// Allocate an array of npages 'struct PageInfo's and store it in 'pages'.
	// The kernel uses this array to keep track of physical pages: for
	// each physical page, there is a corresponding struct PageInfo in this
	// array.  'npages' is the number of physical pages in memory.  Use memset
	// to initialize all fields of each struct PageInfo to 0.
	// Your code goes here:
	pages = (struct PageInfo *) boot_alloc(npages * sizeof(struct PageInfo));
	memset(pages, 0, npages * sizeof(struct PageInfo));
  
  --- Omit some code ---
}
```

In `page_init()` function, we mark pages as free or allocated. 

```c
void
page_init(void) {
	size_t i;

	// Page 0 in use for bootloader data structure
	pages[0].pp_ref = 1;
	// Page [PGSIZE, npages_basemem * PGSIZE] is free
	for (i = 1; i < npages_basemem; i++) {
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
	// Pages from [IOPHYSMEM, EXTPHYSMEM) allocated
	for (i = npages_basemem; i < EXTPHYSMEM / PGSIZE; i++) {
		pages[i].pp_ref = 1;
	}
	// Pages from [address / PGSIZE, npages] free
	size_t address= PADDR((void*)(pages + npages));
	for (int i = address / PGSIZE; i < npages; ++i) {
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}

}
```

In `page_alloc(alloc_flags)` function, allocate new page can be implemented as follows

```c
struct PageInfo *
page_alloc(int alloc_flags)
{
	// page_free_list is empty
	if (!page_free_list) {
		return NULL;
	}

	void *kva = page2kva(page_free_list);
	if (alloc_flags & ALLOC_ZERO) {
		// memset physical page with '\0' byte
		memset(kva, '\0', PGSIZE);
	}
	// Update to next free pages
	struct PageInfo * tmp;
	tmp = page_free_list -> pp_link;
	page_free_list -> pp_link = NULL;
	page_free_list = tmp;
	return pa2page(PADDR(kva));
}
```

In `page_free(pp)` function, we can free an allocated page as follows

```c
void
page_free(struct PageInfo *pp)
{
	// Fill this function in
	// Hint: You may want to panic if pp->pp_ref is nonzero or
	// pp->pp_link is not NULL.
	if (pp->pp_ref != 0 || pp->pp_link) {
		panic("page_free(pp) failed!\n");
	}
	pp->pp_link = page_free_list;
	page_free_list = pp;
}
```

### Exercise 2

#### Question

*Look at chapters 5 and 6 of the [Intel 80386 Reference Manual](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/readings/i386.pdf), if you haven't done so already. Read the sections about page translation and page-based protection closely (5.2 and 6.4). We recommend that you also skim the sections about segmentation; while JOS uses the paging hardware for virtual memory and protection, segment translation and segment-based protection cannot be disabled on the x86, so you will need a basic understanding of it.*

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



