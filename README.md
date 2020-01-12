# Learning Jos From Scratch: Lab5

## Overview

This lab is mainly about **File System**

## Exercise 

### Exercise 1

*`i386_init` identifies the file system environment by passing the type `ENV_TYPE_FS` to your environment creation function, `env_create`. Modify `env_create` in `env.c`, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.*

### Answer

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

