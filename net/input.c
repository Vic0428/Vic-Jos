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
