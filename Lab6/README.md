# Lab 6: Network Driver (default final project)

# 0 Introduction
本实验需要为 the network interface card 编写驱动，所采用的网卡基于 the Intel 82540EM chip（E1000）。

涉及内容包括 the network card driver、network stack、network server。

## 0.1 Getting Started
切换到 lab6 的分支：
```
athena% cd ~/6.828/lab
athena% add git
athena% git pull
athena% git checkout -b lab6 origin/lab6
athena% git merge lab5
...
```
Lab6 新增源文件：
```
net/*
```

# 1 QEMU's virtual network
QEMU 的虚拟内部网络默认赋予 IP 地址 10.0.2.2 给一个虚拟的路由器，以及 IP 地址 10.0.2.15 给 JOS，这些信息写入了文件 net/ns.h。

为了使 QEMU 的虚拟内部网络与外界连接，使 QEMU 运行一个 server 并占据宿主机的某些端口，并能够与 JOS 的某些端口通信。

这样一来，使用 JOS 的端口 7 (echo) and 80 (http) 就能够与外界通信。输入` make which-ports`，可查看占据的宿主机的端口。

## 1.1 Packet Inspection
在 makefile 中已配置使得所有经过的 packets 记录在文件 qemu.pcap 中。

要查看数据报内容，输入`tcpdump -XXnr qemu.pcap`，或使用软件 Wireshark 来打开这个 pcap file。

## 1.2 Debugging the E1000
软件模拟的网卡 E1000 提供了一些友好的调试选项：
```
Flag	Meaning
tx	    Log packet transmit operations
txerr	Log transmit ring errors
rx	    Log changes to RCTL
rxfilter	Log filtering of incoming packets
rxerr	Log receive ring errors
unknown	Log reads and writes of unknown registers
eeprom	Log reads from the EEPROM
interrupt	Log interrupts and changes to interrupt registers.
```
举例：`make E1000_DEBUG=tx,txerr ....`。

E1000 模拟的相关实现代码位于 hw/net/e1000.c。

# 2 The Network Server
JOS 整体网络服务系统结构如下：

![图1](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab6/images/1.png?raw=true)

上图绿颜色的部分是本lab需要实现的部分：
1. E1000网卡驱动，对外提供两个系统调用，用来接收和发送数据。
2. 输入进程
3. 输出进程
4. 用户程序httpd的一部分

JOS 的网络服务器由四个进程组成：
1. core network server environment (includes socket call dispatcher and lwIP)
2. input environment
3. output environment
4. timer environment

## 2.1 The Core Network Server Environment
核心网络服务器 NS 进程由 socket 调用分发器和 lwIP 组成。

其中，lwIP 是一个开源的轻量级 TCP/IP 协议栈实现。对于我们来说 lwIP 就像一个实现了 BSD socket 接口的黑盒，分别有一个包输入和输出端口。

socket调用分发器像 file server 一样工作，负责接收用户进程、输入进程、时钟进程的 IPC 消息，然后通过用户级线程调用合适的 lwIP 接口。

用户进程不直接使用 lib/nsipc.c 中的 nsipc_* 开头的函数，而是使用lib/socket.c中的函数，这些函数基于 file descriptor。所以用户进程通过 file descriptors 来使用 sockets，类似通过 file descriptors 来使用 on-disk files。同理，类似 file server 会为每个 open files 维护一个 ID，lwIP 也会为每一个 open sockets 维护一个 ID，并记录在 struct Fd 中。

另外，NS 进程通过用户级线程来同时提供多个网络服务：如果分发器直接调用 lwIP 的accept和recv阻塞函数，自己也会阻塞，这样就只能提供一个网络服务了。

## 2.2 The Output Environment
LwIP 发送类型为 NSREQ_OUTPUT 的 IPC 消息给输出进程，并将 packet 放入共享页面。

输出进程负责接收消息，然后通过系统调用将这些packets转发给设备驱动。

## 2.3 The Input Environment
对于每个从设备驱动收到的packet，输入进程从内核取出这些packet，然后使用IPC转发给核心网络进程：消息类型 NSREQ_INPUT，packet 放入共享页面。

## 2.4 The Timer Environment
时钟进程周期性地发送类型为 NSREQ_TIMER 的 IPC 消息给核心网络进程，通知它一段时间已经过了，这种消息被lwIP用来实现网络超时。

# 3 Part A: Initialization and transmitting packets
内核目前还没有时间的概念。

硬件每隔10ms都会发送一个时钟中断，每次时钟中断我们可以给某个变量加一，来表明时间过去了10ms，具体实现在kern/time.c中，但还未整合到内核中。

> Exercise 1. 将 time_tick 函数调用整合到内核中，并实现系统调用 sys_time_msec。
 
## 3.1 The Network Interface Card
编写驱动需要很深的硬件以及硬件接口知识。本lab接下来会介绍一些E1000比较高层的知识，但仍需要学会看英特尔的E1000开发者手册。

> Exercise 2. 浏览 Intel's Software Developer's Manual for the E1000。

### 3.1.1 PCI Interface
E1000是PCI设备，意味着E1000将插到主板上的PCI总线上。
PCI总线有地址线，数据线，中断线，从而允许CPU和PCI设备进行交互。

**PCI设备**在被使用前需要被**发现和初始化**：发现的过程是遍历PCI总线寻找相应的设备，初始化的过程是分配I/O和内存空间、协商IRQ线。

kern/pic.c中提供了相关的PCI代码：
在启动阶段，PCI代码遍历PCI总线寻找设备，当它找到一个设备，便会读取该设备的厂商ID和设备ID，然后使用这两个值作为键搜索pci_attach_vendor数组，该数组由struct pci_driver结构组成。
如果找到一个struct pci_driver结构，PCI代码将会执行struct pci_driver结构的attachfn函数指针指向的函数执行初始化。
attachfn函数指针指向的函数参数为一个struct pci_func结构指针，该函数应该调用pci_func_enable()，该函数启动设备，协商资源，并且填充传入的struct pci_func结构。

struct pci_driver结构如下：
```
struct pci_driver {
    uint32_t key1, key2;
    int (*attachfn) (struct pci_func *pcif);
};
```
struct pci_func结构的结构如下：
```
struct pci_func {
    struct pci_bus *bus;

    uint32_t dev;
    uint32_t func;

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};
```
其中最后三项内容：reg_base 数组保存了内存映射I/O的基地址，reg_size 保存了以字节为单位的大小，irq_line 包含了分配的IRQ线。

> Exercise 3. 实现attach函数来初始化E1000，并在kern/pci.c的pci_attach_vendor数组中填入一个表项。82540EM的厂商ID和设备ID可以在手册5.2节找到。

### 3.1.2 Memory-mapped I/O
程序通过内存映射IO（MMIO）和E1000交互，MMIO此前已出现过两次：the CGA console and the LAPIC。

函数 pci_func_enable 负责协商 E1000 使用的 MMIO 范围，并将基地址和大小保存在基地址寄存器 BAR 0（reg_base[0] and reg_size[0]）中。

这是一个很高的物理地址范围(typically above 3GB)，我们需要通过虚拟地址来访问，但无法通过宏 KADDR 来直接转换，所以需要创建一个相应的内存映射：使用函数 mmio_map_region 将其映射到虚拟地址 MMIOBASE 开始的区域，该函数保证了 E1000 所映射的区域不会和 LAPIC 映射的区域冲突。

> Exercise 4. 在 attach 函数中调用 mmio_map_region 来映射物理地址 E1000's BAR 0。

### 3.1.3 DMA
E1000使用DMA技术直接读写内存，不需要CPU参与。

发送和接收队列都由DMA描述符组成，DMA描述符包含一些标志和缓存 packet 的物理地址。

驱动程序负责分配内存作为E1000的发送和接受队列（由DMA描述符构成）、初始化DMA描述符、配置E1000了解这些队列的位置，之后驱动和E1000的操作异步进行。

发送一个 packet：驱动将该 packet 拷贝到发送队列中的一个DMA描述符中，通知E1000，E1000在合适的时间从发送队列的DMA描述符中拿到数据发送出去。

接收一个 packet：E1000将 packet 拷贝到接收队列的一个DMA描述符中，驱动适时从该DMA描述符中读取数据。

发送和接收队列实现为具有头尾指针的环形数组。硬件总是从 head 处取走 descriptors 并移动 head pointer，驱动总是向 tail 处放入 descriptors 并移动 tail pointer。因此，发送队列中的 descriptors 表示待发送的 packets，而接收队列中的 descriptors 表示硬件可用于放入所接收 packets 的 descriptors。

以上告知硬件的指针及地址必须都是物理地址，因为使用 DMA 技术的硬件读写物理内存不通过MMU。

## 3.2 Transmitting Packets
首先使 E1000 支持数据发送的功能：包括初始化（给发送队列分配空间、配置网卡）、完善驱动部分的发送功能。

### 3.2.1 发送过程初始化
初始化过程描述见开发手册 14.5 节。
首先需要初始化发送队列，队列的结构在3.4节，描述符的结构在3.3.3节。

发送队列的环形结构：

![图2](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab6/images/2.png?raw=true)

描述符结构：

![图3](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab6/images/3.png?raw=true)

the legacy transmit descriptor 结构：
```
  63            48 47   40 39   32 31   24 23   16 15             0
  +---------------------------------------------------------------+
  |                         Buffer address                        |
  +---------------+-------+-------+-------+-------+---------------+
  |    Special    |  CSS  | Status|  Cmd  |  CSO  |    Length     |
  +---------------+-------+-------+-------+-------+---------------+

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};
```

初始化时，驱动必须为 transmit descriptor array 和 packet buffers 分配内存空间：复杂高效的做法是动态分配所需空间，简单的方式是静态分配一个极大空间，即预先为每个描述符分配一个对应的数据缓冲区，最大的 Ethernet packet 占1518字节。

>Exercise 5. 按照手册 14.5 节的描述进行初始化。

### 3.2.2 驱动发送数据过程
现在初始化已经完成，接着需要编写代码发送数据包，然后提供系统调用给用户代码使用。

要发送一个 packet，驱动需要将需要将数据拷贝到 packet buffer ，并更新 TDT (transmit descriptor tail) 寄存器来通知网卡新的数据已经就绪。

回收发送队列空间：如果设置了发送描述符 command 区的RS位，那么当网卡发送了这个描述符指向的数据包后，会设置该描述符 status 区的DD位，通过这个标志位就能知道某个描述符是否能被回收。

检测到发送队列已满时的处理：可以简单的丢弃准备发送的数据包，也可以告诉用户进程当前发送队列已满请重新发送。

>Exercise 6. 编写发送函数，负责检查下一个描述符可用、拷贝数据、更新 TDT。

现在可以测试编写的发送功能。尝试从内核调用发送函数发送一些 packets，输入` make E1000_DEBUG=TXERR,TX qemu `会输出以下内容：
```
e1000: index 0: 0x271f00 : 9000002a 0
...
```
每行给出了每个数据在 transmit array 的位置、transmit descriptor 中指向的数据缓冲地址、cmd/CSO/length 区域、special/CSS/status 区域。

QEMU 启动后，输入` tcpdump -XXnr qemu.pcap `来检查所发送的数据。

发送队列和接收队列总结：

![图4](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab6/images/4.png?raw=true)

其中仅 E1000 寄存器使用的空间（如队列头尾指针等）位于MMIOBASE区域，队列实际占用空间及数据缓冲空间由内核统一分配。

### 3.2.3 系统调用接口

>Exercise 7. 实现发送数据包的系统调用.

## 3.3 Transmitting Packets: Network Server
output helper 进程执行一个无限循环，在该循环中接收核心网络进程的IPC请求，解析该请求，然后使用系统调用发送数据。

这个 IPC 消息由 NS 进程调用 net/lwip/jos/jif/jif.c 的 low_level_output 函数发出，类型为 NSREQ_OUTPUT。

IPC 共享页面中包含结构 union Nsipc ，该结构包含存储 packet 的 struct jif_pkt 结构 pkt：
```
struct jif_pkt {
    int jp_len;
    char jp_data[0];
};
``` 
其中，零长度数组 jp_data 是一种表示无需预定义长度的缓存的技巧，因为 C 语言不会检查数组界限，只需确保实际使用中 jp_data 后的空间可用。

注意当输出进程 suspend 在发送系统调用时，NS 进程也会 block 在发送 IPC 给输出进程上，从而其他需要通过 NS 进程发送数据的用户进程也会 block 在发送 IPC 给 NS 进程上。

> Exercise 8. Implement net/output.c.

数据发送流程总结：

![图5](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab6/images/5.png?raw=true)

测试。输入` make E1000_DEBUG=TXERR,TX run-net_testoutput `，输出：
```
Transmitting packet 0
e1000: index 0: 0x271f00 : 9000009 0
Transmitting packet 1
e1000: index 1: 0x2724ee : 9000009 0
...
```
输入` tcpdump -XXnr qemu.pcap `，输出：
```
reading from file qemu.pcap, link-type EN10MB (Ethernet)
-5:00:00.600186 [|ether]
	0x0000:  5061 636b 6574 2030 30                   Packet.00
-5:00:00.610080 [|ether]
	0x0000:  5061 636b 6574 2030 31                   Packet.01
...
```
输入` make E1000_DEBUG=TXERR,TX NET_CFLAGS=-DTESTOUTPUT_COUNT=100 run-net_testoutput`，用以测试数据量更多时的发送情况。

# 4 Part B: Receiving packets and the web server

## 4.1 Receiving Packets
现在使 E1000 支持数据接收的功能：包括初始化（给接收队列分配空间、配置网卡）、完善驱动部分的接收功能。

### 4.1.1 接收过程初始化
类似发送数据，接收数据前也需要配置 E1000 的寄存器、设置接收描述符队列。

手册 3.2 节描述了接收过程、接收队列结构、接收描述符，14.4 节描述了接收初始化过程。

> Exercise 9. 阅读手册 3.2 节。

接收描述符的结构：
```
struct E1000RxDesc {
	uint64_t buffer_addr;
	uint16_t length;    /* Data buffer length */
    uint8_t cso;        /* Checksum offset */

	uint8_t status;
	uint8_t err;
	uint16_t special; 
	
}__attribute__((packed));
```

接收队列结构与发送队列类似。但发送队列中的 descriptors 表示待发送的 packets，而接收队列中的 descriptors 表示硬件可用于放入所接收 packets 的 descriptors。故网络空闲时，发送队列为空，而接收队列为满。

当E1000收到数据包时，首先检查它是否与卡配置的过滤器匹配（例如，查看数据包中MAC地址与网卡是否匹配），如果数据包与任意一个过滤器不匹配，则忽略该数据包。 否则，E1000检查接收队列的头部（RDH）是否已经赶上尾部（RDT），如果接收队列没有空闲描述符，将会丢弃数据包。 如果存在空闲接收描述符，则将分组数据复制到描述符指向的缓冲区中、设置描述符的DD（描述符完成）和EOP（包结束）状态位、递增RDH。

如果E1000接收到大于一个 packet buffer 大小的 packet ，它将从接收队列中检索所需数量的描述符以完整存储数据包，并在所有这些描述符上设置DD状态位，但仅在最后一个描述符上设置EOP状态位。驱动程序必须处理这种情况，或者可以将卡配置为不接受“长数据包”（也称为巨型帧）并确保一个 packet buffer 足够大以存储最大的标准以太网数据包（1518字节）。

> Exercise 10. 设置接收队列，并按 14.4 节配置E1000。E1000目前具体配置为：不支持“长数据包”、不使用中断、去除以太网CRC、设置Receive Address Registers (RAL and RAH)、设置MAC地址为52:54:00:12:34:56等。

测试。输入` make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`，程序 testinput 将会发送一个 ARP 广播帧，然后可以看到输出`e1000: unicast match[0]: 52:54:00:12:34:56` 表明接收到匹配的数据包，而输出`e1000: unicast mismatch: 52:54:00:12:34:56`表明经过的数据被过滤，说明没有正确配置 E1000。

### 4.1.2 驱动接收数据过程
要接收一个 packet，驱动需要将需要将数据拷贝到 packet buffer ，并更新 RDT。

判断 RDT 指向的描述符是否可以被驱动接收：与发送过程相同，通过 status 区的DD标志位来判断。

检测到接收队列（没有数据可被接收，即网络空闲）已满时的处理：可以告诉用户进程当前没有数据可被接收请重新发送，但这种方式不太合适，因为接收队列可能常处于满状态；另一种方式是使调用者进程阻塞直到有数据可被接收，但这种方式需要配置 E1000 开启中断，并使驱动能够处理中断而唤醒被阻塞的进程。

>Exercise 11. 编写驱动接收数据函数，增加相应系统调用接口。

## 4.2 Receiving Packets: Network Server
input 进程执行一个无限循环，在该循环中使用系统调用接收数据，然后向核心网络进程发送一个IPC请求。

这个 IPC 消息类型为 NSREQ_INPUT，共享页面中包含结构 union Nsipc ，该结构包含存储 packet 的 struct jif_pkt 结构 pkt。

> Exercise 12. Implement net/input.c.

测试。输入` make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`，输出：
```
Sending ARP announcement...
Waiting for packets...
e1000: index 0: 0x26dea0 : 900002a 0
e1000: unicast match[0]: 52:54:00:12:34:56
input: 0000   5254 0012 3456 5255  0a00 0202 0806 0001
input: 0010   0800 0604 0002 5255  0a00 0202 0a00 0202
input: 0020   5254 0012 3456 0a00  020f 0000 0000 0000
input: 0030   0000 0000 0000 0000  0000 0000 0000 0000
```

为了更深入的测试，提供了一个守护进程 echosrv。它设置一个在 JOS 的 port 7上运行的echo服务器，将回显通过TCP连接发送的任何内容。在宿主机的一个终端执行` make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-echosrv `开启 echo 服务器，在宿主机的另一个终端上` make nc-7 `连接 echo 服务器。在第二个终端上输入的内容将会被 echo 服务器回显，并且对每个E1000接收到的数据输出信息：
```
e1000: unicast match[0]: 52:54:00:12:34:56
e1000: index 2: 0x26ea7c : 9000036 0
e1000: index 3: 0x26f06a : 9000039 0
e1000: unicast match[0]: 52:54:00:12:34:56
```

## 4.3 用户级线程实现
具体实现在net/lwip/jos/arch/thread.c中。

线程相关数据结构：
```
// thread_queue结构维护两个thread_context指针，分别指向链表的头和尾

struct thread_queue
{
    struct thread_context *tq_first;
    struct thread_context *tq_last;
};

// 每个thread_context结构对应一个线程

struct thread_context {
    thread_id_t     tc_tid;     //线程id
    void        *tc_stack_bottom;       //线程栈
    char        tc_name[name_size];     //线程名
    void        (*tc_entry)(uint32_t);  //线程函数地址
    uint32_t        tc_arg;     //参数
    struct jos_jmp_buf  tc_jb;      //CPU上下文
    volatile uint32_t   *tc_wait_addr;
    volatile char   tc_wakeup;
    void        (*tc_onhalt[THREAD_NUM_ONHALT])(thread_id_t);
    int         tc_nonhalt;
    struct thread_context *tc_queue_link;
};
```

线程系统初始化函数 thread_init(void)：初始化thread_queue全局变量。
```
void
thread_init(void) {
    threadq_init(&thread_queue);
    max_tid = 0;
}

static inline void 
threadq_init(struct thread_queue *tq)
{
    tq->tq_first = 0;
    tq->tq_last = 0;
}
```

线程创建函数 thread_create(thread_id_t tid, const char name, void (*entry)(uint32_t), uint32_t arg)：
```
int
thread_create(thread_id_t *tid, const char *name, 
        void (*entry)(uint32_t), uint32_t arg) {
    struct thread_context *tc = malloc(sizeof(struct thread_context));       //分配一个thread_context结构
    if (!tc)
    return -E_NO_MEM;

    memset(tc, 0, sizeof(struct thread_context));
    
    thread_set_name(tc, name);      //设置线程名
    tc->tc_tid = alloc_tid();       //线程id

    tc->tc_stack_bottom = malloc(stack_size);   //分配独立栈空间，但是一个进程的线程内存是共享的，因为共用一个页表
    if (!tc->tc_stack_bottom) {
        free(tc);
        return -E_NO_MEM;
    }

    void *stacktop = tc->tc_stack_bottom + stack_size;
    // Terminate stack unwinding
    stacktop = stacktop - 4;
    memset(stacktop, 0, 4);
    
    memset(&tc->tc_jb, 0, sizeof(tc->tc_jb));   //设置CPU上下文
    tc->tc_jb.jb_esp = (uint32_t)stacktop;      //esp
    tc->tc_jb.jb_eip = (uint32_t)&thread_entry; //线程代码入口，统一为thread_entry
    tc->tc_entry = entry;                       //线程真正的函数地址
    tc->tc_arg = arg;       //参数

    threadq_push(&thread_queue, tc);    //加入队列中

    if (tid)
    *tid = tc->tc_tid;
    return 0;
}
```

线程切换函数 thread_yield(void)：保存当前线程的寄存器信息到thread_context结构的tc_jb字段中，然后从链表中取下一个thread_context结构，并将其tc_jb字段恢复到对应的寄存器中，继续执行。
```
void
thread_yield(void) {
    struct thread_context *next_tc = threadq_pop(&thread_queue);

    if (!next_tc)
    return;

    if (cur_tc) {
        if (jos_setjmp(&cur_tc->tc_jb) != 0)    //保存当前线程的CPU状态到thread_context结构的tc_jb字段中。
            return;
        threadq_push(&thread_queue, cur_tc);
    }

    cur_tc = next_tc;
    jos_longjmp(&cur_tc->tc_jb, 1); //将下一个线程对应的thread_context结构的tc_jb字段恢复到CPU继续执行
}

// jos_setjmp()和jos_longjmp()需要访问寄存器，由汇编实现

ENTRY(jos_setjmp)
    movl    4(%esp), %ecx   // jos_jmp_buf

    movl    0(%esp), %edx   // %eip as pushed by call
    movl    %edx,  0(%ecx)

    leal    4(%esp), %edx   // where %esp will point when we return
    movl    %edx,  4(%ecx)

    movl    %ebp,  8(%ecx)
    movl    %ebx, 12(%ecx)
    movl    %esi, 16(%ecx)
    movl    %edi, 20(%ecx)

    movl    $0, %eax
    ret

ENTRY(jos_longjmp)
    // %eax is the jos_jmp_buf*
    // %edx is the return value

    movl     0(%eax), %ecx  // %eip
    movl     4(%eax), %esp
    movl     8(%eax), %ebp
    movl    12(%eax), %ebx
    movl    16(%eax), %esi
    movl    20(%eax), %edi

    movl    %edx, %eax
    jmp *%ecx
```

## 4.4 The Web Server
一个最最简单的 Web Server 可以接收客户端请求，并将所请求的文件发送给客户端。

文件 user/httpd.c 已实现了该 Web Server 的整体框架，以及接收并解析请求的部分。

struct http_request 结构如下：
```
struct http_request {
	int sock;
	char *url;
	char *version;
}
```

解析http请求函数：根据请求，解析出一个 struct http_request 结构。
```
// given a request, this function creates a struct http_request
static int
http_request_parse(struct http_request *req, char *request)
{
	const char *url;
	const char *version;
	int url_len, version_len;

	if (!req)
		return -1;

	if (strncmp(request, "GET ", 4) != 0)
		return -E_BAD_REQ;

	// skip GET
	request += 4;

	// get the url
	url = request;
	while (*request && *request != ' ')
		request++;
	url_len = request - url;

	req->url = malloc(url_len + 1);
	memmove(req->url, url, url_len);
	req->url[url_len] = '\0';

	// skip space
	request++;

	version = request;
	while (*request && *request != '\n')
		request++;
	version_len = request - version;

	req->version = malloc(version_len + 1);
	memmove(req->version, version, version_len);
	req->version[version_len] = '\0';

	// no entity parsing

	return 0;
}
```

Web Server 运行一个无限循环，监听端口、读取请求、解析请求、发送文件。
```
void
umain(int argc, char **argv)
{
	int serversock, clientsock;
	struct sockaddr_in server, client;

	binaryname = "jhttpd";

	// Create the TCP socket                // 分配socket
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");

	// Construct the server sockaddr_in structure  /* 初始化服务端地址 */
	memset(&server, 0, sizeof(server));		// Clear struct
	server.sin_family = AF_INET;			// Internet/IP
	server.sin_addr.s_addr = htonl(INADDR_ANY);	// IP address
	server.sin_port = htons(PORT);			// server port

	// Bind the server socket
	if (bind(serversock, (struct sockaddr *) &server,   //绑定socket到服务端地址
		 sizeof(server)) < 0)
	{
		die("Failed to bind the server socket");
	}

	// Listen on the server socket          // 监听socket
	if (listen(serversock, MAXPENDING) < 0)
		die("Failed to listen on server socket");

	cprintf("Waiting for http connections...\n");

	while (1) {
		unsigned int clientlen = sizeof(client);
		// Wait for client connection
		if ((clientsock = accept(serversock,          //接收一个客户端tcp连接，阻塞式函数，分配一个connected socket
					 (struct sockaddr *) &client,
					 &clientlen)) < 0)
		{
			die("Failed to accept client connection");
		}
		handle_client(clientsock);    // http相关，读取请求、解析请求、发送文件
	}

	close(serversock);
}
```
其中，TCP 部分的 socket 编程见 Lwip 实现。tcp 服务端工作原理如图：

图6

http 相关的处理在函数 handle_client 中：
```
static void
handle_client(int sock)
{
	struct http_request con_d;
	int r;
	char buffer[BUFFSIZE];
	int received = -1;
	struct http_request *req = &con_d;

	while (1)
	{
		// Receive message
		if ((received = read(sock, buffer, BUFFSIZE)) < 0)   //读取http请求
			panic("failed to read");

		memset(req, 0, sizeof(*req));

		req->sock = sock;

		r = http_request_parse(req, buffer);    //解析http请求
		if (r == -E_BAD_REQ)
			send_error(req, 400);
		else if (r < 0)
			panic("parse failed");
		else
			send_file(req);                 //发送文件

		req_free(req);

		// no keep alive
		break;
	}

	close(sock);
}
```

> Exercise 13. 完善函数 send_file and send_data，以实现 Web Server 的文件发送部分。

完成后输入`make run-httpd-nox`，在宿主机浏览器中输入`http://localhost:26002`，浏览器会显示404。然后输入`http://localhost:26002/index.html`， Web Server 将会返回一个html文件，网页内容为`cheesy web page`。