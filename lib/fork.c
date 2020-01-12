// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
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

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	extern volatile pte_t uvpt[];
	int perm = 0;

	// If has PTE_SHARE bit set
	if (uvpt[pn] & PTE_SHARE) {
		perm = uvpt[pn] & PTE_SYSCALL;
		// Map to children address space
		if ((r = sys_page_map(0, (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), perm)) < 0) {
			panic("sys_page_map: %e", r);
		}
		return 0;
	}
	else {
		if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
			perm |= PTE_COW;
		}
		perm |= PTE_U | PTE_P;
	}

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

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
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
	for (addr = (uint8_t*) UTEXT; addr < (uint8_t *)(USTACKTOP); addr += PGSIZE) {
		if ((uvpd[PGNUM(addr) >> 10] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)) {
			duppage(envid, PGNUM(addr));
			cprintf("%x\n", addr);
		}
	}


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

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
