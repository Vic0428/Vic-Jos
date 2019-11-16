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

`env_init()` function

```c++
void
env_init(void)
{
	env_free_list = envs;
	// Set up envs array
	for (unsigned int i = 0; i < NENV - 1; i++) {
		envs[i].env_id = 0;
		envs[i].env_link = &envs[i+1];
	}
	// The end of the link
	envs[NENV - 1].env_id = 0;
	envs[NENV - 1].env_link = NULL;
	// Per-CPU part of the initialization
	env_init_percpu();
}
```

`env_setup_vm()` function

```c++
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
	
	// The VA space above UTOP is identiccal
	e->env_pgdir = page2kva(p);
	memcpy(e->env_pgdir, kern_pgdir, PGSIZE);
	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	p->pp_ref += 1;
	return 0;
}

```

`region_alloc()`

```c++
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
	struct PageInfo *p = NULL;
	va = ROUNDDOWN(va, PGSIZE);
	void *end = ROUNDUP(va + len, PGSIZE);
	// Map pages!
	for (int i = 0; i < end - va; i += PGSIZE) {
		p = page_alloc(ALLOC_ZERO);
		if (!p) {
			panic("region_alloc error!");
		}
		page_insert(e->env_pgdir, p, (void *)(va + i), PTE_U | PTE_W);
	}
}
```

`load_icode()` function

```c++
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

	struct Elf *elf = (struct Elf *)binary;
	// is this a valid ELF?
	if (elf->e_magic != ELF_MAGIC) {
		panic("elf file error");
	}
	// Load each program segment
	struct Proghdr *ph = (struct Proghdr *)(binary + elf->e_phoff);
	// End of program segment
	struct Proghdr *eph = ph + elf->e_phnum;

	// Change to user pagedir
	lcr3(PADDR(e->env_pgdir));

	for (; ph < eph; ph++) {
		if (ph->p_type == ELF_PROG_LOAD) {
			// Allocate memory
			region_alloc(e, (void *)ph->p_va, ph->p_memsz);
			// Copy memory
			memcpy((void *)ph->p_va, (void *)(binary + ph->p_offset), (size_t)ph->p_filesz);
			// Clear bss section
			memset((void *)(ph->p_va + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
		}
	}

	// Set entry point
	(e->env_tf).tf_eip = (uintptr_t)elf->e_entry;
	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.
	region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);

	// Change back to kern pgdir
	lcr3(PADDR(kern_pgdir));
}
```

`env_create()` function

```c++
void
env_create(uint8_t *binary, enum EnvType type)
{
	struct Env *e;
	if (env_alloc(&e, 0) < 0) {
		panic("env_alloc error!");
	}
	load_icode(e, binary);
	e->env_type = type;
}
```

`env_run()` function

```c++
void
env_run(struct Env *e)
{
	// Step 1: If this is a context switch (a new environment is running):
	//	   1. Set the current environment (if any) back to
	//	      ENV_RUNNABLE if it is ENV_RUNNING (think about
	//	      what other states it can be in),
	//	   2. Set 'curenv' to the new environment,
	//	   3. Set its status to ENV_RUNNING,
	//	   4. Update its 'env_runs' counter,
	//	   5. Use lcr3() to switch to its address space.
	// Step 2: Use env_pop_tf() to restore the environment's
	//	   registers and drop into user mode in the
	//	   environment.

	// Hint: This function loads the new environment's state from
	//	e->env_tf.  Go back through the code you wrote above
	//	and make sure you have set the relevant parts of
	//	e->env_tf to sensible values.

	// LAB 3: Your code here.
	if (curenv != NULL) {
		if (curenv->env_status == ENV_RUNNING) {
			curenv->env_status = ENV_RUNNABLE;

		}
	}
	curenv = e;
	curenv->env_status = ENV_RUNNING;
	curenv->env_runs += 1;

	lcr3(PADDR(curenv->env_pgdir));
	// We need to set e->env_tf.tf_eip! 
	env_pop_tf(&curenv->env_tf);

}
```



