# Learning Jos From Scratch: Lab1

## Overview

This lab is mainly about **PC Bootstrap** and **GC Calling Conventions**. 

## Getting Started

### Mac OS

#### Build Your Own Complier Toolchain

First, install `brew`

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)”
```

Then, install the toolchains

```
brew install i386-elf-gcc i386-elf-gdb
```

#### QEMU Simulator

First, specify that we want use tap from [HMC CS134](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/)

```shell
brew tap nrhodes/homebrew-os134sp19
```

Then, install qemu with

```
brew install nrhodes/homebrew-os134sp19/qemu
```

Sometimes, you may need to enter following command to use `i386-elf-gdb`

```
brew link --overwrite i386-elf-gdb
```

And you can enter following command to use you own gdb

```
brew link --overwrite gdb
```

## Exercise

### Exercise 1

#### Question

Familiarize yourself with the assembly language materials available on [the CS 134 reference page](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/reference.html). You don't have to read them now, but you'll almost certainly want to refer to some of this material when reading and writing x86 assembly.

We do recommend reading the section "The Syntax" in [Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html). It gives a good (and quite brief) description of the AT&T assembly syntax we'll be using with the GNU assembler in JOS.

#### Answer

Read book to learn how to read x86 assembly. 

### Exercise 2

#### Question

Use GDB's si (Step Instruction) command to trace into the ROM BIOS for a few more instructions, and try to guess what it might be doing. You might want to look at [Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm), as well as other materials on the [CS 134 reference materials page](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/reference.html). No need to figure out all the details - just the general idea of what the BIOS is doing first.

#### Answer

Learn GDB and keep *step in* !

### Exercise 3

#### Question

Take a look at the [lab tools guide](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/labguide.html), especially the section on GDB commands. Even if you're familiar with GDB, this includes some esoteric GDB commands that are useful for OS work.

Set a breakpoint at address 0x7c00, which is where the boot sector will be loaded. Continue execution until that breakpoint. Trace through the code in `boot/boot.S`, using the source code and the disassembly file `obj/boot/boot.asm` to keep track of where you are. Also use the `x/i` command in GDB to disassemble sequences of instructions in the boot loader, and compare the original boot loader source code with both the disassembly in `obj/boot/boot.asm` and GDB.

Trace into `bootmain()` in `boot/main.c`, and then into `readsect()`. Identify the exact assembly instructions that correspond to each of the statements in `readsect()`. Trace through the rest of `readsect()` and back out into `bootmain()`, and identify the begin and end of the `for` loop that reads the remaining sectors of the kernel from the disk. Find out what code will run when the loop is finished, set a breakpoint there, and continue to that breakpoint. Then step through the remainder of the boot loader.

#### Answer

Keep using gdb `si` command and `x/I` command can disassemble sequences of instructions. We can find following things:

- The point that the processor starts executing 32-bit code and that the professor switches from 16 to 32-bit mode. 

- The *last* instruction of the boot loader executed and the *first* instruction of the kernel. 

- The boot loader uses following code to fetch the entire kernel from disk. 

  ```c
  // load each program segment (ignores ph flags)
  ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
  eph = ph + ELFHDR->e_phnum;
  for (; ph < eph; ph++)
    // p_pa is the load address of this segment (as well
    // as the physical address)
    readseg(ph->p_pa, ph->p_memsz, ph->p_offset)
  ```

### Exercise 4

#### Question

Read about programming with pointers in C. The best reference for the C language is *The C Programming Language* by Brian Kernighan and Dennis Ritchie (known as 'K&R'). We recommend that students purchase this book (here is an [Amazon Link](http://www.amazon.com/C-Programming-Language-2nd/dp/0131103628/sr=8-1/qid=1157812738/ref=pd_bbs_1/104-1502762-1803102?ie=UTF8&s=books)) or find one of [MIT's 7 copies](http://library.mit.edu/F/AI9Y4SJ2L5ELEE2TAQUAAR44XV5RTTQHE47P9MKP5GQDLR9A8X-10422?func=item-global&doc_library=MIT01&doc_number=000355242&year=&volume=&sub_library=).

Read 5.1 (Pointers and Addresses) through 5.5 (Character Pointers and Functions) in K&R. Then download the code for [pointers.c](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/labs/lab1/pointers.c), run it, and make sure you understand where all of the printed values come from. In particular, make sure you understand where the pointer addresses in printed lines 1 and 6 come from, how all the values in printed lines 2 through 4 get there, and why the values printed in line 5 are seemingly corrupted.

There are other references on pointers in C (e.g., [A tutorial by Ted Jensen](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/readings/pointers.pdf) that cites K&R heavily), though not as strongly recommended.

#### Answer

*Pointer is the spirit of C.*

### Exercise 5

#### Question

Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in `boot/Makefrag` to something wrong, run make clean, recompile the lab with make, and trace into the boot loader again to see what happens. Don't forget to change the link address back and make clean again afterward!

#### Answer

Change the boot loader’s link address will cause error because BIOS will transfer control to invalid address. 

### Exercise 6

#### Question

We can examine memory using GDB's x command. The [GDB manual](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html) has full details, but for now, it is enough to know that the command x/*N*x *ADDR* prints *N* words of memory at *ADDR*. (Note that both '`x`'s in the command are lowercase.) *Warning*: The size of a word is not a universal standard. In GNU assembly, a word is two bytes (the 'w' in xorw, which stands for word, means 2 bytes).

Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.)

#### Answer

At the first break point, it will all be 0’s. It’s because that the kernel hasn’t been loader.  And at the second break point, it will contain information about kernel. 

## Reference

- [Setting Up Environment](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/tools.html)
- [GCC-Inline-Assembly](http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)