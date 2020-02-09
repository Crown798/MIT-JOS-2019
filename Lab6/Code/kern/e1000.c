
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
volatile void *bar_va;

#define E1000REG(offset) (void *)(bar_va + offset)

struct e1000_tdh *tdh;            // 用于指向MMIO物理内存空间的硬件寄存器，虚拟地址位于MMIOBASE开始的区域，不可使用KADDR和PADDR转换
struct e1000_tdt *tdt;            // 用于指向MMIO物理内存空间的硬件寄存器，虚拟地址位于MMIOBASE开始的区域，不可使用KADDR和PADDR转换
struct e1000_tx_desc tx_desc_array[TXDESCS];      // 内核分配的物理内存空间，虚拟地址位于KERNBASE之上，可使用KADDR和PADDR转换
char tx_buffer_array[TXDESCS][TX_PKT_SIZE];       // 内核分配的物理内存空间，虚拟地址位于KERNBASE之上，可使用KADDR和PADDR转换

struct e1000_rdh *rdh;
struct e1000_rdt *rdt;
struct e1000_rx_desc rx_desc_array[RXDESCS];
char rx_buffer_array[RXDESCS][RX_PKT_SIZE];

uint32_t E1000_MAC[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

int
e1000_attachfn(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);     //从线性地址MMIOBASE开始映射物理地址pa开始的size大小的内存，并返回pa对应的线性地址

    uint32_t *status_reg = (uint32_t *)E1000REG(E1000_STATUS);
    assert(*status_reg == 0x80080783);                                  //确保寄存器状态正确

    e1000_transmit_init();
    e1000_receive_init();

    return 0;
}

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

int
e1000_transmit(void *data, size_t len)
{
       uint32_t current = tdt->tdt;     //tail index in queue
       if(!(tx_desc_array[current].status & E1000_TXD_STAT_DD)) {
               return -E_TRANSMIT_RETRY;
       }
       tx_desc_array[current].length = len;
       tx_desc_array[current].status &= ~E1000_TXD_STAT_DD;
       tx_desc_array[current].cmd |= (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
       memcpy(tx_buffer_array[current], data, len);
       uint32_t next = (current + 1) % TXDESCS;
       tdt->tdt = next;
       return 0;
}

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

void
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

int
e1000_receive(void *addr, size_t *len)
{
       static int32_t next = 0;
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
