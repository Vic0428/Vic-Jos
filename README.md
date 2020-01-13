# Learning Jos From Scratch: Lab5

## Overview

This lab is mainly about **File System**

## Exercise 

### Exercise 1

#### Question

*`i386_init` identifies the file system environment by passing the type `ENV_TYPE_FS` to your environment creation function, `env_create`. Modify `env_create` in `env.c`, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.*

#### Answer

Modify `env_create()` function

```c
void
env_create(uint8_t *binary, enum EnvType type)
{
	// LAB 3: Your code here.

	// If this is the file server (type == ENV_TYPE_FS) give it I/O privileges.
	// LAB 5: Your code here.
	struct Env *e;
	if (env_alloc(&e, 0) < 0) {
		panic("env_alloc error!");
	}
	if (type == ENV_TYPE_FS) {
		e->env_tf.tf_eflags |= FL_IOPL_3;
	}
	// Enable interrupts
	load_icode(e, binary);
	e->env_type = type;
}

```

### Exercise 2

#### Question

*Implement the `bc_pgfault` and `flush_block` functions in `fs/bc.c`. `bc_pgfault` is a page fault handler, just like the one your wrote in the previous lab for copy-on-write fork, except that its job is to load pages in from the disk in response to a page fault. When writing this, keep in mind that (1) `addr` may not be aligned to a block boundary and (2) `ide_read` operates in sectors, not blocks.*

#### Answer

`bc_pgfault()`

```c++
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:
	// Round the addr
	addr = ROUNDDOWN(addr, PGSIZE);
	// Allocate a page
	if ((r = sys_page_alloc(0, addr, PTE_P | PTE_W | PTE_U) < 0)) {
		panic("sys_page_alloc error!\n");
	};
	// Read from disk
	if ((r = ide_read(blockno * BLKSECTS, addr, BLKSECTS)) < 0) {
		panic("ide_read error!\n");
	}

	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}
```

`flush_block()`

```c++
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	addr = ROUNDDOWN(addr, PGSIZE);
	if (!va_is_mapped(addr) || !va_is_dirty(addr)) {
		return;
	}
	if (va_is_mapped(addr) && va_is_dirty(addr)) {
		// write to blockno
		if ((r = ide_write(blockno * BLKSECTS, addr, BLKSECTS)) < 0) {
			panic("ide_write error!");
		};
		// Clear PTE_D bit
		if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
			panic("in bc_pgfault, sys_page_map: %e", r);
	}
}
```

### Exercise 3

#### Question

*Use `free_block` as a model to implement `alloc_block` in `fs/fs.c`, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with `flush_block`, to help file system consistency.*

#### Answer

`alloc_block()`

```c++
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.

	// LAB 5: Your code here.
	for (uint32_t blockno = 1; blockno < super->s_nblocks; blockno++) {
		// Find free block
		if (block_is_free(blockno)) {
			// Mark it as used
			bitmap[blockno/32] &= ~(1 << (blockno%32));
			// Flush the block
			flush_block(bitmap + (blockno / 32));
			// Return block NO
			return blockno;
		}
	}
	return -E_NO_DISK;
}
```

### Exercise 4

#### Question

*Implement `file_block_walk` and `file_get_block`. `file_block_walk` maps from a block offset within a file to the pointer for that block in the `struct File` or the indirect block, very much like what `pgdir_walk` did for page tables. `file_get_block` goes one step further and maps to the actual disk block, allocating a new one if necessary*

#### Answer

`file_get_block()`

```c++
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
	uint32_t *ptr;
	int r;
	// If file block meets error!
	if ((r = file_block_walk(f, filebno, &ptr, 1)) < 0) {
		return r;
	};
	// Allocate block for this entry
	if (*ptr == 0) {
		if ((*ptr = alloc_block()) < 0) {
			return -E_NO_DISK;
		};
	}
	*blk = (char *)diskaddr(*ptr);
	return 0;
}
```

`file_block_walk()`

```c++
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
	// If filebno is out of range
	if (filebno >= NDIRECT + NINDIRECT) {
		return -E_INVAL;
	}
  // Check invalid pointer
	if (!ppdiskbno) {
		return 0;
	}
	if (filebno < NDIRECT) {
		*ppdiskbno = &f->f_direct[filebno];
	} else {
		// The indirect slot hasn't been allocated
		if (f->f_indirect == 0)	 {
			if (alloc == 0)	 {
				return -E_NOT_FOUND;
			} else {
				if ((f->f_indirect = alloc_block()) < 0) {
					return -E_NO_DISK;
				}
			}
    	memset(diskaddr(f->f_indirect), 0, BLKSIZE);
			flush_block(diskaddr(f->f_indirect));
		}
		filebno -= NDIRECT;

		// Allocate a block to corresponding entry
		uint32_t *addr = (uint32_t *)f->f_indirect;
		*ppdiskbno = &addr[filebno];
	}
	return 0;
}
```

### Exercise 5

#### Question

*Implement `serve_read` in `fs/serv.c`.*

#### Answer

`serve_read()`

```c++
int
serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;
	struct OpenFile *o;
	int r;
	// Look up open files
	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0) {
		return r;
	}
	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);
	// Read bytes into buffer
	r = file_read(o->o_file, (void *)ret->ret_buf, req->req_n, o->o_fd->fd_offset);
	if (r < 0) {
		return r;
	} else {
		o->o_fd->fd_offset += r;
	}
	return r;
}
```

### Exercise 6

#### Question

*Implement `serve_write` in `fs/serv.c` and `devfile_write` in `lib/file.c`.*

#### Answer

`devfile_write()`

```c++
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	// Make an FSREQ_WRITE request to the file system server.  Be
	// careful: fsipcbuf.write.req_buf is only so large, but
	// remember that write is always allowed to write *fewer*
	// bytes than requested.
	int r;
	fsipcbuf.write.req_fileid = fd->fd_file.id;
	fsipcbuf.write.req_n = n;
	memmove(fsipcbuf.write.req_buf, buf, n);
	return fsipc(FSREQ_WRITE, NULL);
}
```

`serve_write()`

```c++
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);
	struct OpenFile *o;
	int r;
	// Look up open files
	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0) {
		return r;
	}
	// Extend files if necessary
	if (o->o_fd->fd_offset + req->req_n > o->o_file->f_size) {
		file_set_size(o->o_file, o->o_fd->fd_offset + req->req_n);
	}
	// Write to corresponding file
	r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset);
	if (r >= 0) {
		o->o_fd->fd_offset += r;
	}
	return r;
}
```

### Exercise 7

#### Question

*`spawn` relies on the new syscall `sys_env_set_trapframe` to initialize the state of the newly created environment. Implement `sys_env_set_trapframe` in `kern/syscall.c` (don't forget to dispatch the new system call in `syscall()`.*

#### Answer

`sys_env_set_trapframe()`

```c++
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	// Bad address
	if (!tf) {
		panic("Bad address!\n");
	}
	e->env_tf = *tf;
	// Set IOPL of 0
	e->env_tf.tf_eflags |= FL_IOPL_0;
	// Enable interrupts
	// CPL 3
	return 0;
}
```

### Exercise 8

#### Question

*Change `duppage` in `lib/fork.c` to follow the new convention. If the page table entry has the `PTE_SHARE` bit set, just copy the mapping directly. (You should use `PTE_SYSCALL`, not `0xfff`, to mask out the relevant bits from the page table entry. `0xfff` picks up the accessed and dirty bits as well.)*

*Likewise, implement `copy_shared_pages` in `lib/spawn.c`. It should loop through all page table entries in the current process (just like `fork` did), copying any page mappings that have the `PTE_SHARE` bit set into the child process.*

#### Answer

`duppage()`

```c++
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
```

`copy_shared_pages()`

```c++
static int
copy_shared_pages(envid_t child)
{
	int r;
	for (size_t pn = PGNUM(UTEXT); pn < PGNUM(USTACKTOP); pn++) {
		if ((uvpd[pn >> 10] & PTE_P) && (uvpt[pn] & PTE_P)) {
			if (uvpt[pn] & PTE_SHARE) {
				// Map shared pages
				if ((r = sys_page_map(0, (void*)(pn*PGSIZE), child, (void*)(pn*PGSIZE), uvpt[pn] & PTE_SYSCALL)) < 0) {
					panic("sys_page_map error!\n");
				}
			}
		}
	}
	return 0;
}

```

### Exercise 9

#### Question

*In your `kern/trap.c`, call `kbd_intr` to handle trap `IRQ_OFFSET+IRQ_KBD` and `serial_intr` to handle trap `IRQ_OFFSET+IRQ_SERIAL`.*

#### Answer

```c++
case IRQ_OFFSET + IRQ_KBD:
	kbd_intr();
	break;
case IRQ_OFFSET + IRQ_SERIAL:
	serial_intr();
	break;
```

### Exercise 10

#### Question

*The shell doesn't support I/O redirection. It would be nice to run sh <script instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for < to `user/sh.c`.*

#### Answer

```c++
case '<':	// Input redirection
  // Grab the filename from the argument list
  if (gettoken(0, &t) != 'w') {
    cprintf("syntax error: < not followed by word\n");
    exit();
  }
  // Open 't' for reading as file descriptor 0
  // (which environments use as standard input).
  // We can't open a file onto a particular descriptor,
  // so open the file as 'fd',
  // then check whether 'fd' is 0.
  // If not, dup 'fd' onto file descriptor 0,
  // then close the original 'fd'.

  // LAB 5: Your code here.
  if ((fd = open(t, O_RDONLY)) < 0) {
    cprintf("open %s for read: %e", t, fd);
    exit();
  }
  if (fd != 0) {
    dup(fd, 0);
    close(fd);
  }
  break;
```

