Exercise 12. Implement net/input.c.

# 代码

```
void
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
```
