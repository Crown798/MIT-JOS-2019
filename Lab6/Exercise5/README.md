>Exercise 5. Perform the initialization steps described in section 14.5 (but not its subsections). Use section 13 as a reference for the registers the initialization process refers to and sections 3.3.3 and 3.4 for reference to the transmit descriptors and transmit descriptor array.

Be mindful of the alignment requirements on the transmit descriptor array and the restrictions on length of this array. Since TDLEN must be 128-byte aligned and each transmit descriptor is 16 bytes, your transmit descriptor array will need some multiple of 8 transmit descriptors. However, don't use more than 64 descriptors or our tests won't be able to test transmit ring overflow.

For the TCTL.COLD, you can assume full-duplex operation. For TIPG, refer to the default values described in table 13-77 of section 13.4.34 for the IEEE 802.3 standard IPG (don't use the values in the table in section 14.5).

Try running make E1000_DEBUG=TXERR,TX qemu. If you are using the course qemu, you should see an "e1000: tx disabled" message when you set the TDT register (since this happens before you set TCTL.EN) and no further "e1000" messages.

# 分析
14.5 节的初始化步骤如下：
1. 分配一块内存用作发送描述符队列，起始地址要16字节对齐。用基地址填充(TDBAL/TDBAH) 寄存器。
2. 设置(TDLEN)寄存器，保存发送描述符队列长度，必须128字节对齐。
3. 设置(TDH/TDT)寄存器，保存发送描述符队列的头部和尾部，应该初始化为0。
4. 初始化TCTL寄存器：设置TCTL.EN位为1，设置TCTL.PSP位为1。设置TCTL.CT为10h。设置TCTL.COLD为40h。
5. 设置TIPG寄存器。

# 代码

注意仅 E1000 寄存器使用的空间位于MMIOBASE区域。
该初始化函数编写完成后应加入 e1000_attachfn 函数中。
```
struct e1000_tdh *tdh;            // 用于指向MMIO物理内存空间的硬件寄存器，虚拟地址位于MMIOBASE开始的区域，不可使用KADDR和PADDR转换
struct e1000_tdt *tdt;            // 用于指向MMIO物理内存空间的硬件寄存器，虚拟地址位于MMIOBASE开始的区域，不可使用KADDR和PADDR转换
struct e1000_tx_desc tx_desc_array[TXDESCS];      // 内核分配的物理内存空间，虚拟地址位于KERNBASE之上，可使用KADDR和PADDR转换
char tx_buffer_array[TXDESCS][TX_PKT_SIZE];       // 内核分配的物理内存空间，虚拟地址位于KERNBASE之上，可使用KADDR和PADDR转换

void
e1000_transmit_init()
{
       int i;
       for (i = 0; i < TXDESCS; i++) {
               tx_desc_array[i].addr = PADDR(tx_buffer_array[i]);
               tx_desc_array[i].cmd = 0;
               tx_desc_array[i].status |= E1000_TXD_STAT_DD;
       }
    //设置队列长度寄存器
       struct e1000_tdlen *tdlen = (struct e1000_tdlen *)E1000REG(E1000_TDLEN);
       tdlen->len = TXDESCS;
             
    //设置队列基址低32位
       uint32_t *tdbal = (uint32_t *)E1000REG(E1000_TDBAL);
       *tdbal = PADDR(tx_desc_array);                           // PADDR可以得到物理地址吗？可。

    //设置队列基址高32位
       uint32_t *tdbah = (uint32_t *)E1000REG(E1000_TDBAH);
       *tdbah = 0;

    //设置头指针寄存器
       tdh = (struct e1000_tdh *)E1000REG(E1000_TDH);
       tdh->tdh = 0;

    //设置尾指针寄存器
       tdt = (struct e1000_tdt *)E1000REG(E1000_TDT);
       tdt->tdt = 0;

    //TCTL register
       struct e1000_tctl *tctl = (struct e1000_tctl *)E1000REG(E1000_TCTL);
       tctl->en = 1;
       tctl->psp = 1;
       tctl->ct = 0x10;
       tctl->cold = 0x40;

    //TIPG register
       struct e1000_tipg *tipg = (struct e1000_tipg *)E1000REG(E1000_TIPG);
       tipg->ipgt = 10;
       tipg->ipgr1 = 4;
       tipg->ipgr2 = 6;
}
```