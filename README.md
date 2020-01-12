# Learning Jos From Scratch: Lab4

## Overview

This lab is mainly about **Preemptive Multitasking**. 

## Exercise 

### Exercise 1

#### Question

*Implement `mmio_map_region` in `kern/pmap.c`. To see how this is used, look at the beginning of `lapic_init` in `kern/lapic.c`. You'll have to do the next exercise, too, before the tests for `mmio_map_region` will run*

#### Answer

`mmio_map_region()`

```c++
void *
mmio_map_region(physaddr_t pa, size_t size)
{
	// Compute next base address
	uintptr_t next_base = ROUNDUP(base + size, PGSIZE);

	// Round up and Round down
	base = ROUNDDOWN(base, PGSIZE);

	// The end of pa address (ROUNDUP)
	physaddr_t pa_end = ROUNDUP(pa + size, PGSIZE);
	// The beginning of pa address (ROUNDOWN)
	pa = ROUNDDOWN(pa, PGSIZE);
	// The size of mapping rnage
	size = (size_t)(pa_end - pa);

	// Handle the overflow case!
	if (base + size > MMIOLIM || pa_end < pa) {
		panic("mmio_map_region() error!\n");
	}

	// Map region
	boot_map_region(kern_pgdir, base, size, pa, PTE_PCD | PTE_PWT | PTE_W);

	// Update base address
	base = next_base;

	return (void *)prev_base;
}
```

### Exercise 2

#### Question

*Read `boot_aps()` and `mp_main()` in `kern/init.c`, and the assembly code in `kern/mpentry.S`. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of `page_init()` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated `check_page_free_list()` test (but might fail the updated `check_kern_pgdir()` test, which we will fix soon)*

#### Answer

```c++
void
page_init(void)
{
	size_t i;

	// Page 0 in use for bootloader data structure
	pages[0].pp_ref = 1;
	// Page [PGSIZE, npages_basemem * PGSIZE] is free
	for (i = 1; i < npages_basemem; i++) {
		// Avoid adding the page at MPENTRY_PADDR to the free list
		if (i == MPENTRY_PADDR / PGSIZE) {
			continue;
		}
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
	(--- omit some code here ---)
}
```

### Exercise 3

#### Question

*Modify `mem_init_mp()` (in `kern/pmap.c`) to map per-CPU stacks starting at `KSTACKTOP`, as shown in `inc/memlayout.h`. The size of each stack is `KSTKSIZE` bytes plus `KSTKGAP` bytes of unmapped guard pages. Your code should pass the new check in `check_kern_pgdir()`.*

#### Answer

```c++
static void
mem_init_mp(void)
{
	for (int i = 0; i < NCPU; i++) {
		uintptr_t kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
		boot_map_region(kern_pgdir, kstacktop_i - KSTKSIZE, KSTKSIZE, 
						PADDR(percpu_kstacks[i]), PTE_W);
	}
}

```

### Exercise 4

#### Question

*The code in `trap_init_percpu()` (`kern/trap.c`) initializes the TSS and TSS descriptor for the BSP. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global `ts` variable any more.)*

#### Answer

```c
// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - You won't want to load GD_TSS0: that's the GD_TSS for cpu 0.
	//     The definition of gdt[] shows where the GD_TSSes are stored for
	//     other cpus.
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	int i = cpunum();
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + i].sd_s = 0;

	ltr(GD_TSS0 + (i << 3));
	// Load the IDT
	lidt(&idt_pd);
}

```

### Exercise 5

#### Question

*Apply the big kernel lock as described above, by calling `lock_kernel()` and `unlock_kernel()` at the proper locations.*

#### Answer

`i386_init()`

```
lock_kernel();
boot_aps();
```

`mp_main()`

```
lock_kernel();
sched_yield();
```

`trap()`

```
lock_kernel();
assert(curenv);
```

`env_run()`

```
lcr3(PADDR(e->env_pgdir));
unlock_kernel();
env_pop_tf(&(e->env_tf));
```

### Exercise 6

#### Question

*Implement round-robin scheduling in `sched_yield()` as described above. Don't forget to modify `syscall()` to dispatch `sys_yield()`.*

#### Answer

```c
// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.
	int i;
	int id = curenv - envs;
	// cprintf("CPU: %d, ENV: %x\n", cpunum(), curenv);
	if (curenv == NULL) {
		// Search Runnable environment
		for (i = 0; i < NENV; i++) {
			if (envs[i].env_status == ENV_RUNNABLE) {
				env_run(&envs[i]);
			}
		}
	} else {
		for (i = id + 1; i < NENV + id; i++) {
			if (envs[i % NENV].env_status == ENV_RUNNABLE) {
				env_run(&envs[i % NENV]);
			}
		}
		if (curenv->env_status == ENV_RUNNING) {
				env_run(curenv);
		}
	}
	// sched_halt never returns
	sched_halt();
}
```

### Exercise 7

#### Question

*Implement the system calls described above in `kern/syscall.c` and make sure `syscall()` calls them. You will need to use various functions in `kern/pmap.c` and `kern/env.c`, particularly `envid2env()`. For now, whenever you call `envid2env()`, pass 1 in the `checkperm` parameter. Be sure you check for any invalid system call arguments, returning `-E_INVAL` in that case. Test your JOS kernel with `user/dumbfork` and make sure it works before proceeding.*

#### Answer

`sys_exofork()`

```c++
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
	struct Env *e;
	int r;
	// Error case: allocate failed
	if ((r = env_alloc(&e, curenv->env_id)) < 0) {
		return r;
	}
	// Set env status
	e->env_status = ENV_NOT_RUNNABLE;
	// Copy regoster set
	e->env_tf = curenv->env_tf;
	// Tweak!
	e->env_tf.tf_regs.reg_eax = 0;
	return e->env_id;
}
```

`sys_env_set_status()`

```c++
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	struct Env* e;
	// Error case 1: environment doesn't exist
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Error case 2: the status is not valid
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
		return -E_INVAL;
	}
	// Change the status
	e->env_status = status;
	return 0;
}
```

`sys_page_alloc()`

```c++
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!
	struct Env *e;
	// Error case 1: bad environemnt
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Error case 2: bad address
	if ((uintptr_t)va >= UTOP || PGOFF(va)) {
		return -E_INVAL;
	}
	if ((perm & PTE_U) && (perm & PTE_P) && !(perm & ~PTE_SYSCALL)) {
		struct PageInfo *pp = page_alloc(ALLOC_ZERO);
		// Error case 4: out of memory
		if (pp == NULL) {
			return -E_NO_MEM;
		}
		// Return page insert result
		if (page_insert(e->env_pgdir, pp, va, perm) < 0) {
			page_free(pp);
			return -E_NO_MEM;
		}
		return 0;
	} else {
		// Error case 3: bad permission
		return -E_INVAL;
	}
}
```

`sys_page_map()`

```c++
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	struct Env *srcEnv, *dstEnv;
	// Bad case 1: src env or dst env wrong 
	if (envid2env(srcenvid, &srcEnv, 1) < 0) {
		return -E_BAD_ENV;
	}
	if (envid2env(dstenvid, &dstEnv, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Bad case 2: invalid address
	if ((uintptr_t)srcva >= UTOP || PGOFF(srcva) || (uintptr_t)dstva >= UTOP || PGOFF(dstva)) {
		return -E_INVAL;
	}
	// Bad case 3: perm error
	if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL)) {
		return -E_INVAL;
	}
	// Bad case 4: src addr is not mapped
	struct PageInfo *pp;
	pte_t* pte;
	pp = page_lookup(srcEnv->env_pgdir, srcva, &pte);
	if (pp == NULL) {
		return -E_INVAL;
	}
	// Bad case 5: src va is readonly but dst is read and write
	if (!(*pte & PTE_W) && (perm & PTE_W)) {
		return -E_INVAL;
	}
	return page_insert(dstEnv->env_pgdir, pp, dstva, perm);
}
```

`sys_page_unmap()`

```c++
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
	// Bad case 1: va >= UTOP, or not page-aligned
	if ((uintptr_t)va >= UTOP || PGOFF(va)) {
		return -E_INVAL;
	}
	// Bad case 2: environemnt envid doesn't curently exist
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Good case 1: remove the page
	page_remove(e->env_pgdir, va);
	return 0;
}
```

### Exercise 8

#### Question

*Implement the `sys_env_set_pgfault_upcall` system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call.*

#### Answer

`sys_env_set_pgfault_upcall()`

```c++
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	struct Env *e;
	// Error case 1: bad environemnt
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Set env's 'env_pgfault_upcall' field
	e->env_pgfault_upcall = func;
	return 0;
}
```

### Exercise 9

#### Question

*Implement the code in `page_fault_handler` in `kern/trap.c` required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)*

#### Answer

In `page_fault_handler()` function

```c++
if (!curenv->env_pgfault_upcall) {
		cprintf("[%08x] user fault va %08x ip %08x\n",
			curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
	} else {

		// Set up uFrame on kernel stack
		struct UTrapframe uFrame;
		uFrame.utf_eflags = tf->tf_eflags;
		uFrame.utf_eip = tf->tf_eip;
		uFrame.utf_err = tf->tf_err;
		uFrame.utf_esp = tf->tf_esp;
		uFrame.utf_fault_va = fault_va;
		uFrame.utf_regs = tf->tf_regs;

		// Set the next instruction
		tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;

		// Check tf->tf_eip
		user_mem_assert(curenv, (void *)tf->tf_eip, PGSIZE, PTE_U | PTE_P);

		// Already on user exception stack
		if (tf->tf_esp >= UXSTACKTOP - PGSIZE && tf->tf_esp <= UXSTACKTOP - 1) {
			// Push up an empty 32-bit word
			tf->tf_esp -= 4;
			tf->tf_esp -= sizeof(struct UTrapframe);
			// Error: expection stack overflow !!!
			if (tf->tf_esp < UXSTACKTOP - PGSIZE) {
				env_destroy(curenv);
			}
		} else {
			tf->tf_esp = UXSTACKTOP - sizeof(struct UTrapframe);
		}

		// Check tf-tf_esp
		user_mem_assert(curenv, (void *)(tf->tf_esp), sizeof(struct UTrapframe), PTE_W | PTE_U | PTE_P);

		// Copy UTrapfram from kernel stack to user exception stack
		struct UTrapframe *ptr = (struct UTrapframe *)tf->tf_esp;
		*ptr = uFrame;

		// Return to user environment
		env_run(curenv);
	}
```

### Exercise 10

#### Question

*Implement the `_pgfault_upcall` routine in `lib/pfentry.S`. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP.*

#### Answer

```assembly
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument

	// Skip fault_va, tf_err
	addl $0x8, %esp
	// %eax = trap-time eip 
	movl 32(%esp), %eax
	// %ecx = trap-time esp - 4
	movl 40(%esp), %ecx
	subl $4, %ecx
	// *(esp-4) = eip
	movl %eax, (%ecx)
	// adjust trap-time stack
	movl %ecx, 40(%esp)

	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	popal
	addl $4, %esp

	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	popfl

	// Switch back to the adjusted trap-time stack.

	pop %esp

	// Return to re-execute the instruction that faulted.
	ret
```

### Exercise 11

#### Question

*Finish `set_pgfault_handler()` in `lib/pgfault.c`.*

#### Answer

`set_pgfault_handler()`

```c++
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
		// Allocate exception stack
		if (sys_page_alloc(thisenv->env_id, (void*)UXSTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P) < 0) {
			panic("Allocate exception stack failed!\n");
		}
		// Set entry point
		sys_env_set_pgfault_upcall(thisenv->env_id, _pgfault_upcall);
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}
```

### Exercise 12

#### Question

*Implement `fork`, `duppage` and `pgfault` in `lib/fork.c`.*

#### Answer

`pgfault()`

```c++
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	extern volatile pte_t uvpt[];


	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	// cprintf("error code: %p\n", err);
	// cprintf("faulting address: %p\n", addr);
	// cprintf("eip: %p\n", utf->utf_eip);

	if ((err & FEC_PR)== 0) {
		panic("pgfault() error1!");
	}
	if ((uvpt[PGNUM(addr)] & PTE_COW) == 0) {
		panic("pgfault() error2!");
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// Allocate a new page
	sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W);
	// Copy from the old page to the new page
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	// map new page
	sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_U | PTE_W);
	sys_page_unmap(0, PFTEMP);

}
```

`duppage()`

```c++
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	extern volatile pte_t uvpt[];
	int perm = 0;
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
		perm |= PTE_COW;
	}
	perm |= PTE_U | PTE_P;
	// Map to children address space
	if ((r = sys_page_map(0, (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), perm)) < 0) {
		panic("sys_page_map: %e", r);
	}
	// Map to parents address space
	if ((r = sys_page_map(0, (void*)(pn*PGSIZE), 0, (void*)(pn*PGSIZE), perm)) < 0) {
		panic("sys_page_map: %e", r);
	}
	return 0;
}
```

`fork()`

```c++
envid_t
fork(void)
{
	// Installs the c level page handler
	set_pgfault_handler(pgfault);
	envid_t envid;
	uint8_t *addr;
	extern unsigned char end[];
	int r;

	// Allocate a new child environment
	envid = sys_exofork();
	if (envid < 0) {
		panic("sys_exofork: %e", envid);
	}
	if (envid == 0) {
		// We're the child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// Copy page mapping
	for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE) {
		duppage(envid, PGNUM(addr));
	}

	// Map new page for stack
	duppage(envid, PGNUM(USTACKTOP-PGSIZE));

	// Allocate new page for and entry point for children
	if ((r = sys_page_alloc(envid, (void*)UXSTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P)) < 0) {
		panic("sys_page_alloc: %e", r);
	}
	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(envid, &_pgfault_upcall)) < 0) {
		panic("sys_env_set_pgfault_upcall: %e", r);
	}

	// We're the parent
	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);
	return envid;
}
```

### Exercise 13

#### Question

*Modify `kern/trapentry.S` and `kern/trap.c` to initialize the appropriate entries in the IDT and provide handlers for IRQs 0 through 15. Then modify the code in `env_alloc()` in `kern/env.c` to ensure that user environments are always run with interrupts enabled.*

#### Answer

In `env_alloc()` function

```
e->env_tf.tf_eflags = FL_IF;
```

### Exercise 14

#### Question

*Modify the kernel's `trap_dispatch()` function so that it calls `sched_yield()` to find and run a different environment whenever a clock interrupt takes place.*

#### Answer

In `trap_dispatch()` function

```c++
case IRQ_TIMER + IRQ_OFFSET:
    lapic_eoi();
    sched_yield();
```

### Exercise 15

#### Question

*Implement `sys_ipc_recv` and `sys_ipc_try_send` in `kern/syscall.c`. Read the comments on both before implementing them, since they have to work together. When you call `envid2env` in these routines, you should set the `checkperm` flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid.*

#### Answer

```c++
static int
sys_ipc_recv(void *dstva)
{
	// dstva < UTOP but dstva is not page-aligned
	if ((uint32_t)dstva < UTOP && PGOFF(dstva)) {
		return -E_INVAL;
	}
	// Record info
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;

	// Mask current environment not runnable
	curenv->env_status = ENV_NOT_RUNNABLE;
	// Giveup the CPU
	sched_yield();
}
```

```c++
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	struct Env* e;
	// Envid doesn't currently exist
	if (envid2env(envid, &e, 0) < 0) {
		return -E_BAD_ENV;
	}
	// The target not blocked for waiting for IPC
	if (!e->env_ipc_recving ){
		return -E_IPC_NOT_RECV;
	}
	// srcva is not page-aligned
	if ((uint32_t)srcva < UTOP && PGOFF(srcva)) {
		return -E_INVAL;
	}
	// permission error!
	if ((uint32_t)srcva < UTOP) {
		if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL)) {
			return -E_INVAL;
		}
	}
	// srcva < UTOP but not mapped
	if ((uint32_t)srcva < UTOP && (uint32_t)e->env_ipc_dstva < UTOP) {
		struct PageInfo *pp;
		pte_t* pte;
		pp = page_lookup(curenv->env_pgdir, srcva, &pte);
		if (pp == NULL) {
			return -E_INVAL;
		}
		// if (perm & PTE_W), but srcva is read-only 
		if (!(*pte & PTE_W) && (perm & PTE_W)) {
			return -E_INVAL;
		}
		// Map page!
		if (page_insert(e->env_pgdir, pp, e->env_ipc_dstva, perm) < 0) {
			return -E_NO_MEM;
		}
	}
	// Succeed!
	e->env_ipc_recving = 0;
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_value = value;
	e->env_ipc_perm = (uint32_t)srcva < UTOP? perm: 0;
	e->env_status = ENV_RUNNABLE;
	e->env_tf.tf_regs.reg_eax = 0;

	return 0;

}
```



```c++
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// pg == null => mark pg as UTOP
	pg = pg? pg: (void *)UTOP;
	while (1) {
		int ret = sys_ipc_try_send(to_env, val, pg, perm);
		if (ret == 0) {
			break;
		}
		if (ret < 0 && ret != -E_IPC_NOT_RECV) {
			panic("ipc_send() error!");
		}
		sys_yield();
	}
}
```

```c++
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	pg = pg? pg: (void *)UTOP;
	int ret = sys_ipc_recv(pg);
	if (from_env_store) {
		*from_env_store = ret < 0? 0: thisenv->env_ipc_from;
	}
	if (perm_store) {
		*perm_store = ret < 0? 0: thisenv->env_ipc_perm;
	}
	return ret < 0? ret: thisenv->env_ipc_value;
}
```







