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

