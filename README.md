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
	load_icode(e, binary);
	e->env_type = type;
	if (type == ENV_TYPE_FS) {
		e->env_tf.tf_eflags |= FL_IOPL_3;
	}
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
	if ((r = sys_page_alloc(0, addr, PTE_SYSCALL) < 0)) {
		panic("sys_page_alloc error!\n");
	};
	// Read from disk
	if ((r = ide_read(blockno * 8, addr, 8)) < 0) {
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
	// write to blockno
	if ((r = ide_write(blockno * 8, addr, 8)) < 0) {
		panic("ide_write error!");
	};
	// Clear PTE_D bit
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);
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
		if (bitmap[blockno/32] & (1 << (blockno%32))) {
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



