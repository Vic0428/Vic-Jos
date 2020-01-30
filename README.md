# Learning Jos From Scratch: Lab6

## Overview

The lab is mainly about **Network driver**

## Exercise

### Exercise 1

#### Question

*Add a call to `time_tick` for every clock interrupt in `kern/trap.c`. Implement `sys_time_msec` and add it to `syscall` in `kern/syscall.c` so that user space has access to the time.*

#### Answer

Modify in `kern/trap.c`

```c++
	if (tf->tf_trapno == IRQ_OFFSET) {
		time_tick();
		lapic_eoi();
		sched_yield();	
	}
```

And add a new system call

```c++
static int
sys_time_msec(void)
{
	return time_msec();
}
```

### Exercise 2

#### Question

*Implement an attach function to initialize the E1000. Add an entry to the `pci_attach_vendor` array in `kern/pci.c` to trigger your function if a matching PCI device is found (be sure to put it before the `{0, 0, 0}` entry that mark the end of the table). You can find the vendor ID and device ID of the 82540EM that QEMU emulates in section 5.2. You should also see these listed when JOS scans the PCI bus while booting.*

#### Answer

Modify `kern/pci.c`

```c++
// pci_attach_vendor matches the vendor ID and device ID of a PCI device. key1
// and key2 should be the vendor ID and device ID respectively
struct pci_driver pci_attach_vendor[] = {
	{E1000_VENDOR_ID, E1000_DEVICE_ID, &pci_e1000_attach},
	{ 0, 0, 0 },
};
```

Add a new function in `kern/e1000.c`

```c++
int pci_e1000_attach(struct pci_func *f) {
    pci_func_enable(f);
    return 0;
}
```

### Exercise 3

#### Exercise

*In your attach function, create a virtual memory mapping for the E1000's BAR 0 by calling `mmio_map_region` (which you wrote in lab 4 to support memory-mapping the LAPIC).*

#### Answer

Add new line in `pic_e1000_attach` function

```c++
e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);
```

### Exercise 4

#### Exercise

*Perform the initialization steps described in section 14.5 (but not its subsections). Use section 13 as a reference for the registers the initialization process refers to and sections 3.3.3 and 3.4 for reference to the transmit descriptors and transmit descriptor array.*

#### Answer

In `kern/e1000.c`

```c++
struct tx_desc tda[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{0, 0, 0, 0, 0, 0, 0}};
struct packet tx_buffer[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{{0}}};

int pci_e1000_attach(struct pci_func *f) {
    pci_func_enable(f);
    e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);

    // Transmit initialization
    for (int i = 0; i < TDA_COUNT; ++i) {
        tda[i].addr = PADDR(&tx_buffer[i]);
        tda[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        tda[i].status = E1000_TXD_STAT_DD;
    }
    E1000_LOCATE(e1000, E1000_TDBAL) = PADDR(tda);
    E1000_LOCATE(e1000, E1000_TDLEN) = sizeof(struct tx_desc) * TDA_COUNT;
    E1000_LOCATE(e1000, E1000_TDH) = 0;
    E1000_LOCATE(e1000, E1000_TDT) = 0;
    E1000_LOCATE(e1000, E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) | (0x40 << 12);
    E1000_LOCATE(e1000, E1000_TIPG) = 10 | (8 << 10) | (12 << 20);
    return 0;
}
```

### Exercise 5

#### Question

*Write a function to transmit a packet by checking that the next descriptor is free, copying the packet data into the next descriptor, and updating TDT. Make sure you handle the transmit queue being full.*

#### Answer

In `kern/e1000.c`

```c++
int e1000_transmit(void *addr, size_t len) {
    uint32_t tail = E1000_LOCATE(e1000, E1000_TDT);
    if (len > PACKET_MAX_SIZE) {
        len = PACKET_MAX_SIZE;
    }
    if (!(tda[tail].status & E1000_TXD_STAT_DD)) {
        return -1;
    } else {
        memmove(&tx_buffer[tail], addr, len);
        // Copy data into packet
        tda[tail].status &= ~E1000_TXD_STAT_DD;
        tda[tail].length = len;
        // Move the tail
        E1000_LOCATE(e1000, E1000_TDT) = (tail + 1) % TDA_COUNT;
    }
    return 0;
}
```

### Exercise 6

#### Question

*Add a system call that lets you transmit packets from user space. The exact interface is up to you. Don't forget to check any pointers passed to the kernel from user space.*

#### Answer

Add new system call

```c++
static int
sys_netpacket_try_send(void *addr, size_t len) {
	user_mem_assert(curenv, addr, len, PTE_U);
	return e1000_transmit(addr, len);
}
```

### Exercise 7

#### Question

*Implement `net/output.c`.*

#### Answer

```c++
#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int perm;
	envid_t eid;
	while(1) {
		if (ipc_recv(&eid, &nsipcbuf, &perm) != NSREQ_OUTPUT) {
			continue;
		}
		if (sys_netpacket_try_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len) < 0) {
			sys_yield();
		}
	}
}

```

### Exercise 8

#### Question

*Set up the receive queue and configure the E1000 by following the process in section 14.4. You don't have to support "long packets" or multicast. For now, don't configure the card to use interrupts; you can change that later if you decide to use receive interrupts. Also, configure the E1000 to strip the Ethernet CRC, since the grade script expects it to be stripped.*

#### Answer

Add code to `pci_e1000_attach` function in `kern/e1000.c`

```c++
    // Receive initialization
    // Configure mac address
    for (int i = 0; i < RDA_COUNT; ++i) {
        rda[i].buffer_addr = PADDR(&rx_buffer[i]);
    }

    uint32_t low = 0;
    for (int i = 0; i < 4; i++) {
        low |= (MAC[i] << (8 * i));
    }
    uint32_t high = 0;
    for (int i = 0; i < 2; i++) {
        high |= (MAC[i + 4] << (8 * i));
    }
    E1000_LOCATE(e1000, E1000_RA) = low;
    E1000_LOCATE(e1000, E1000_RA + 4) = high | E1000_RAH_AV;

    E1000_LOCATE(e1000, E1000_ICS) = 0;
    E1000_LOCATE(e1000, E1000_IMS) = 0;

    E1000_LOCATE(e1000, E1000_RDBAL) = PADDR(rda);
    E1000_LOCATE(e1000, E1000_RDBAH) = 0;

    E1000_LOCATE(e1000, E1000_RDLEN) = RDA_COUNT * sizeof(struct rx_desc);
    E1000_LOCATE(e1000, E1000_RDH) = 0;
    E1000_LOCATE(e1000, E1000_RDT) = RDA_COUNT - 1;

    E1000_LOCATE(e1000, E1000_RCTL) = E1000_RCTL_EN  | E1000_RCTL_SECRC | E1000_RCTL_SZ_2048 | E1000_RCTL_BAM;
```

### Exercise 9

#### Question

*Write a function to receive a packet from the E1000 and expose it to user space by adding a system call. Make sure you handle the receive queue being empty.*

#### Answer

Add following function in `kern/e1000.c`

```c++
int e1000_receive(void *addr, size_t len) {
    uint32_t tail = (E1000_LOCATE(e1000, E1000_RDT) + 1) % RDA_COUNT;
    if (!(rda[tail].status & E1000_RXD_STAT_DD)) {
        return -1;
    } else {
        if (len > rda[tail].length) {
            len = rda[tail].length;
        }
        memmove(addr, &rx_buffer[tail], len);
        rda[tail].status &= ~E1000_RXD_STAT_DD;
        // Move the head
        E1000_LOCATE(e1000, E1000_RDT) = tail;
    }
    return len;
}
```

And add new system call

```c++
static int
sys_netpacket_try_receive(void *addr, size_t len) {
	user_mem_assert(curenv, addr, len, PTE_U);
	return e1000_receive(addr, len);
}
```

### Exercise 10

#### Question

*Implement `net/input.c`.*

#### Answer

```c++
#include "ns.h"
#define BUFFER_SIZE 2048
extern union Nsipc nsipcbuf;

void sleep(int msec) {
	unsigned now = sys_time_msec();
	unsigned end = sys_time_msec() + msec;

	while (sys_time_msec() < end) {
		sys_yield();
	}
}
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r;

	while(1) {
		char buffer[BUFFER_SIZE];
		while((r = sys_netpacket_try_receive(buffer, BUFFER_SIZE)) < 0) {
			sys_yield();
		}
		nsipcbuf.pkt.jp_len = r;
		memcpy(nsipcbuf.pkt.jp_data, buffer, r);
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_P);
		sleep(50);
	}
}

```

### Exercise 11

#### Question

*The web server is missing the code that deals with sending the contents of a file back to the client. Finish the web server by implementing `send_file` and `send_data`.*

#### Answer

Modify `send_data` function in `user/httpd.c`

```c++
static int
send_data(struct http_request *req, int fd)
{
	char buf[128];
	int r;
	while (1) {
		r = read(fd, buf, 128);
		if (r <= 0) {
			return r;
		}
		if (write(req->sock, buf, r) != r) {
			return -1;
		}
	}
	return 0;
}
```

Add code to `send_file` function in `user/httpd.c`

```c++
if ((fd = open(req->url, O_RDONLY)) < 0) {
	send_error(req, 404);
	goto end;
}
if ((r = fstat(fd, &st)) < 0) {
	send_error(req, 404);
	goto end;
}
if (st.st_isdir) {
	send_error(req, 404);
	goto end;
}
```



