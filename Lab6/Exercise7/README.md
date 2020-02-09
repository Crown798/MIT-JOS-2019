>Exercise 7. Add a system call that lets you transmit packets from user space. The exact interface is up to you. Don't forget to check any pointers passed to the kernel from user space.

# 代码

```
// inc/syscall.h
SYS_pkt_try_send


// kern/syscall.c
static int 
sys_pkt_try_send(void * buf, size_t len)
{
	user_mem_assert(curenv, buf, len, PTE_U);
	return e1000_transmit(buf, len);
}

case SYS_pkt_try_send:
		return sys_pkt_try_send((void *) a1, (size_t) a2);


// lib/syscall.c
int
sys_pkt_try_send(void *addr, size_t len)
{
	return syscall(SYS_pkt_try_send, 0, (uint32_t)addr, (uint32_t)len, 0, 0, 0);
}


// inc/lib.h
int sys_pkt_try_send(void *addr, size_t len);
```