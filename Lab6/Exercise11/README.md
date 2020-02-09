>Exercise 11. Write a function to receive a packet from the E1000 and expose it to user space by adding a system call. Make sure you handle the receive queue being empty.

# 代码

驱动接收数据函数：
```
int
e1000_receive(void *addr, size_t *len)
{
       static int32_t next = 0;              // static 变量计数，随使用的描述符环形递增
       if(!(rx_desc_array[next].status & E1000_RXD_STAT_DD)) {	//simply tell client to retry
               return -E_RECEIVE_RETRY;
       }
       if(rx_desc_array[next].errors) {
               cprintf("receive errors\n");
               return -E_RECEIVE_RETRY;
       }
       *len = rx_desc_array[next].length;
       memcpy(addr, rx_buffer_array[next], *len);

       rdt->rdt = (rdt->rdt + 1) % RXDESCS;      //add tail index
       next = (next + 1) % RXDESCS;
       return 0;
}
```

系统调用接口：
```
// inc/syscall.h
SYS_pkt_recv


// kern/syscall.c
static int
sys_pkt_recv(void *addr, size_t *len)
{
	return e1000_receive(addr, len);
}

case SYS__pkt_recv:
		ret = sys_pkt_try_send((void *) a1, (size_t *) a2);
		break;


// lib/syscall.c
int
sys_pkt_recv(void *addr, size_t *len)
{
	return syscall(SYS_pkt_recv, 1, (uint32_t)addr, (uint32_t)len, 0, 0, 0);
}


// inc/lib.h
int sys_pkt_recv(void *addr, size_t *len);
```