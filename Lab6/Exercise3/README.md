>Exercise 3. Implement an attach function to initialize the E1000. Add an entry to the pci_attach_vendor array in kern/pci.c to trigger your function if a matching PCI device is found (be sure to put it before the {0, 0, 0} entry that mark the end of the table). You can find the vendor ID and device ID of the 82540EM that QEMU emulates in section 5.2. You should also see these listed when JOS scans the PCI bus while booting.

For now, just enable the E1000 device via pci_func_enable. We'll add more initialization throughout the lab.

We have provided the kern/e1000.c and kern/e1000.h files for you so that you do not need to mess with the build system. They are currently blank; you need to fill them in for this exercise. You may also need to include the e1000.h file in other places in the kernel.

When you boot your kernel, you should see it print that the PCI function of the E1000 card was enabled. Your code should now pass the pci attach test of make grade.

# 代码

kern/e1000.c 中的 attach 函数:
```
int
e1000_attachfn(struct pci_func *pcif)
{
       pci_func_enable(pcif);
       return 0;
}
```

82540EM的厂商ID和设备ID可以在手册5.2节找到.
```
#define E1000_VENDOR_ID_82540EM 0x8086
#define E1000_DEV_ID_82540EM 0x100E
```

kern/pci.c 中的 pci_attach_vendor 表：
```
 struct pci_driver pci_attach_vendor[] = {
       { E1000_VENDER_ID_82540EM, E1000_DEV_ID_82540EM, &e1000_attachfn },
        { 0, 0, 0 },
 };
```