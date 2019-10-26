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

