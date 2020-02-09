>Exercise 10. Set up the receive queue and configure the E1000 by following the process in section 14.4. You don't have to support "long packets" or multicast. For now, don't configure the card to use interrupts; you can change that later if you decide to use receive interrupts. Also, configure the E1000 to strip the Ethernet CRC, since the grade script expects it to be stripped.

By default, the card will filter out all packets. You have to configure the Receive Address Registers (RAL and RAH) with the card's own MAC address in order to accept packets addressed to that card. You can simply hard-code QEMU's default MAC address of 52:54:00:12:34:56 (we already hard-code this in lwIP, so doing it here too doesn't make things any worse). Be very careful with the byte order; MAC addresses are written from lowest-order byte to highest-order byte, so 52:54:00:12 are the low-order 32 bits of the MAC address and 34:56 are the high-order 16 bits.

The E1000 only supports a specific set of receive buffer sizes (given in the description of RCTL.BSIZE in 13.4.22). If you make your receive packet buffers large enough and disable long packets, you won't have to worry about packets spanning multiple receive buffers. Also, remember that, just like for transmit, the receive queue and the packet buffers must be contiguous in physical memory.

You should use at least 128 receive descriptors

# 代码

分配接收描述符队列空间、配置 E1000 的寄存器：
```
struct e1000_rdh *rdh;
struct e1000_rdt *rdt;
struct e1000_rx_desc rx_desc_array[RXDESCS];
char rx_buffer_array[RXDESCS][RX_PKT_SIZE];

uint32_t E1000_MAC[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

static void
set_ra_address(uint32_t mac[], uint32_t *ral, uint32_t *rah)
{
       uint32_t low = 0, high = 0;
       int i;

       for (i = 0; i < 4; i++) {
               low |= mac[i] << (8 * i);
       }

       for (i = 4; i < 6; i++) {
               high |= mac[i] << (8 * i);
       }

       *ral = low;
       *rah = high | E1000_RAH_AV;
}

static void
e1000_receive_init()
{       //设置接收队列基址寄存器
       uint32_t *rdbal = (uint32_t *)E1000REG(E1000_RDBAL);
       uint32_t *rdbah = (uint32_t *)E1000REG(E1000_RDBAH);
       *rdbal = PADDR(rx_desc_array);
       *rdbah = 0;

       int i;
       for (i = 0; i < RXDESCS; i++) {
               rx_desc_array[i].addr = PADDR(rx_buffer_array[i]);
       }
        //设置接收队列长度寄存器
       struct e1000_rdlen *rdlen = (struct e1000_rdlen *)E1000REG(E1000_RDLEN);
       rdlen->len = RXDESCS;

	//设置头尾指针寄存器
       rdh = (struct e1000_rdh *)E1000REG(E1000_RDH);
       rdt = (struct e1000_rdt *)E1000REG(E1000_RDT);
       rdh->rdh = 0;
       rdt->rdt = RXDESCS-1;

       uint32_t *rctl = (uint32_t *)E1000REG(E1000_RCTL);
       *rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC;

        //除了本网卡的物理地址的包都过滤掉
       uint32_t *ra = (uint32_t *)E1000REG(E1000_RA);
       set_ra_address(E1000_MAC, ra, ra + 1);
}
```
