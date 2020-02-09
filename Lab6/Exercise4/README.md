>Exercise 4. In your attach function, create a virtual memory mapping for the E1000's BAR 0 by calling mmio_map_region (which you wrote in lab 4 to support memory-mapping the LAPIC).

You'll want to record the location of this mapping in a variable so you can later access the registers you just mapped. Take a look at the lapic variable in kern/lapic.c for an example of one way to do this. If you do use a pointer to the device register mapping, be sure to declare it volatile; otherwise, the compiler is allowed to cache values and reorder accesses to this memory.

To test your mapping, try printing out the device status register (section 13.4.2). This is a 4 byte register that starts at byte 8 of the register space. You should get 0x80080783, which indicates a full duplex link is up at 1000 MB/s, among other things.

# 代码

映射的虚拟地址存放在 bar_va 中。映射空间用于与 E1000 寄存器的内容交互。
kern/e1000.c 中的 attach 函数:
```
volatile void *bar_va;

#define E1000REG(offset) (void *)(bar_va + offset)

int
e1000_attachfn(struct pci_func *pcif)
{
       pci_func_enable(pcif);
       bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);     //从线性地址MMIOBASE开始映射物理地址pa开始的size大小的内存，并返回pa对应的线性地址

       uint32_t *status_reg = (uint32_t *)E1000REG(E1000_STATUS);
       assert(*status_reg == 0x80080783);                                  //确保寄存器状态正确
       return 0;
 }
```

```
#define E1000_STATUS   0x00008  /* Device Status - RO */
```


