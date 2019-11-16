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



### Exercise3

#### Question

 Read Chapter 5 of the [IA-32 Developer's Manual](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/readings/ia32/IA32-3A.pdf) if you haven't already.

#### Answer

Carefully read the book!

### Exercise 4

#### Question

Edit `trapentry.S` and `trap.c` and implement the features described above. The macros `TRAPHANDLER` and `TRAPHANDLER_NOEC` in `trapentry.S` should help you, as well as the T_* defines in `inc/trap.h`. You will need to add an entry point in `trapentry.S` (using those macros) for each trap defined in `inc/trap.h`, and you'll have to provide `_alltraps` which the `TRAPHANDLER` macros refer to. You will also need to modify `trap_init()` to initialize the `idt` to point to each of these entry points defined in `trapentry.S`; the `SETGATE` macro will be helpful here.

Your `_alltraps` should:

1. push values to make the stack look like a struct Trapframe
2. load `GD_KD` into `%ds` and `%es`
3. `pushl %esp` to pass a pointer to the Trapframe as an argument to trap()
4. `call trap` (can `trap` ever return?)

Consider using the `pushal` instruction; it fits nicely with the layout of the `struct Trapframe`.

Test your trap handling code using some of the test programs in the `user` directory that cause exceptions before making any system calls, such as `user/divzero`. You should be able to get make grade to succeed on the `divzero`, `softint`, and `badsegment` tests at this point.

#### Answer

Modify `trapentry.s` 

```asm
TRAPHANDLER_NOEC(divide_handler, 0)
TRAPHANDLER_NOEC(debug_handler, 1)
TRAPHANDLER_NOEC(nmi_handler, 2)
TRAPHANDLER_NOEC(brkpt_handler, 3)
TRAPHANDLER_NOEC(oflow_handler, 4)
TRAPHANDLER_NOEC(bound_handler, 5)
TRAPHANDLER_NOEC(device_handler, 7)
TRAPHANDLER_NOEC(illop_handler, 6)
TRAPHANDLER(dblflt_handler, 8)
TRAPHANDLER(tss_handler, 10)
TRAPHANDLER(segnp_handler, 11)
TRAPHANDLER(stack_handler, 12)
TRAPHANDLER(gpflt_handler, 13)
TRAPHANDLER(pgflt_handler, 14)
TRAPHANDLER_NOEC(fperr_handler, 16)
TRAPHANDLER(align_handler, 17)
TRAPHANDLER_NOEC(mchk_handler, 18)
TRAPHANDLER_NOEC(simderr_handler, 19)
TRAPHANDLER_NOEC(syscall_handler, 48)
```

Modify `trap_init()` function in `trap.c`

```c++
void
trap_init(void)
{
	extern struct Segdesc gdt[];
	// LAB 3: Your code here.
	void divide_handler();
	void debug_handler();
	void nmi_handler();
	void brkpt_handler();
	void oflow_handler();
	void bound_handler();
	void device_handler();
	void illop_handler();
	void tss_handler();
	void segnp_handler();
	void stack_handler();
	void gpflt_handler();
	void pgflt_handler();
	void fperr_handler();
	void align_handler();
	void mchk_handler();
	void simderr_handler();
	void syscall_handler();
	void dblflt_handler();

  SETGATE(idt[T_DIVIDE], 0, GD_KT, divide_handler, 0);
  SETGATE(idt[T_DEBUG], 0, GD_KT, debug_handler, 0);
  SETGATE(idt[T_NMI], 0, GD_KT, nmi_handler, 0);
  SETGATE(idt[T_BRKPT], 0, GD_KT, brkpt_handler, 0);
  SETGATE(idt[T_OFLOW], 0, GD_KT, oflow_handler, 0);
  SETGATE(idt[T_BOUND], 0, GD_KT, bound_handler, 0);
  SETGATE(idt[T_DEVICE], 0, GD_KT, device_handler, 0);
  SETGATE(idt[T_ILLOP], 0, GD_KT, illop_handler, 0);
  SETGATE(idt[T_DBLFLT], 0, GD_KT, dblflt_handler, 0);
  SETGATE(idt[T_TSS], 0, GD_KT, tss_handler, 0);
  SETGATE(idt[T_SEGNP], 0, GD_KT, segnp_handler, 0);
  SETGATE(idt[T_STACK], 0, GD_KT, stack_handler, 0);
  SETGATE(idt[T_GPFLT], 0, GD_KT, gpflt_handler, 0);
  SETGATE(idt[T_PGFLT], 0, GD_KT, pgflt_handler, 0);
  SETGATE(idt[T_FPERR], 0, GD_KT, fperr_handler, 0);
  SETGATE(idt[T_ALIGN], 0, GD_KT, align_handler, 0);
  SETGATE(idt[T_MCHK], 0, GD_KT, mchk_handler, 0);
  SETGATE(idt[T_SIMDERR], 0, GD_KT, simderr_handler, 0);
  
  SETGATE(idt[T_SYSCALL], 0, GD_KT, syscall_handler, 3);

	// Per-CPU setup 
	trap_init_percpu();
}
```

#### Question

1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)
2. Did you have to do anything to make the `user/softint` program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but `softint`'s code says `int $14`. *Why* should this produce interrupt vector 13? What happens if the kernel actually allows `softint`'s `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

### Answer

1. Every interrupt should be handled differently. 
2. Because current mode is user mode, if the code says `int $14`. It will cause *General Protection Exception*. 