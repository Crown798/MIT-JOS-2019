>Exercise 8. Implement net/output.c.

# 代码

```
void
output(envid_t ns_envid)
{
    binaryname = "ns_output";

    // LAB 6: Your code here:
    //  - read a packet from the network server
    //  - send the packet to the device driver
    uint32_t whom;
    int perm;
    int32_t req;

    while (1) {
        req = ipc_recv((envid_t *)&whom, &nsipcbuf,  &perm);     //接收核心网络进程发来的请求
        if (req != NSREQ_OUTPUT) {
            cprintf("not a nsreq output\n");
            continue;
        }

        struct jif_pkt *pkt = &(nsipcbuf.pkt);
        while (sys_pkt_try_send(pkt->jp_data, pkt->jp_len) < 0) {        //通过系统调用发送数据包，多次尝试直到发送成功
            sys_yield();                                             //发送失败时切换
        }   
    }
}
```
