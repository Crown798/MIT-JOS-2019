#include "ns.h"
#include <kern/e1000.h>
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

static void
sleep(int msec)
{
	unsigned now = sys_time_msec();
	unsigned end = now + msec;

	if (end < now)
		panic("sleep: wrap");

	while (sys_time_msec() < end)
		sys_yield();
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
	size_t len;
	char buf[RX_PKT_SIZE];
	while (1) {
			while (sys_pkt_recv(buf, &len) < 0) {
				sys_yield();
			}
			memcpy(nsipcbuf.pkt.jp_data, buf, len);
			nsipcbuf.pkt.jp_len = len;

			ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_U|PTE_W);
			sleep(50);
	}
}
