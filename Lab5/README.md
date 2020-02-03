# Lab 5: File system, Spawn and Shell

# 0 Introduction
本实验涉及文件系统、从磁盘装载并运行程序（库函数 spawn）、shell。

## 0.1 Getting Started
切换到 lab5 的分支：
```
athena% cd ~/6.828/lab
athena% add git
athena% git pull
athena% git checkout -b lab5 origin/lab5
athena% git merge lab4
...
```
Lab5 新增源文件：
```
fs/fs.c	Code that mainipulates the file system's on-disk structure.
fs/bc.c	A simple block cache built on top of our user-level page fault handling facility.
fs/ide.c	Minimal PIO-based (non-interrupt-driven) IDE driver code.
fs/serv.c	The file system server that interacts with client environments using file system IPCs.
lib/fd.c	Code that implements the general UNIX-like file descriptor interface.
lib/file.c	The driver for on-disk file type, implemented as a file system IPC client.
lib/console.c	The driver for console input/output file type.
lib/spawn.c	Code skeleton of the spawn library call.
```

# 1 File system preliminaries
JOS 将要实现的文件系统相比 xv6 更加简化：支持基本的创建、读写、删除、层次目录结构，不支持所属用户、权限、硬链接、软链接、时间戳、特殊设备。

## 1.1 On-Disk File System Structure
JOS的文件系统不使用inodes，所有文件的元数据都被存储在directory entry中。JOS文件系统允许用户读目录元数据，这就意味着用户可以扫描目录来像实现ls这种程序，但这种方式使得应用程序过度依赖目录元数据格式。

普通文件和目录文件都是由一系列 blocks 组成，这些blocks分散在磁盘中。文件系统屏蔽blocks分布的细节，提供一个可以顺序读写文件的接口。

# 1.2 Sectors and Blocks
文件系统以 block 为单位，磁盘以 sector 为单位。

JOS中，block size 为4096字节，sector size 为512字节。

# 1.3 Superblocks
superblocks 存储文件系统的元数据，如 block size, disk size, 根目录位置等。

JOS 的文件系统使用一个superblock，位于磁盘的block 1。真实的文件系统维护多个superblock，这样当一个损坏时，依然可以正常运行。

block 0被用来保存boot loader和分区表。

Super结构如下：
```
struct Super {
    uint32_t s_magic;       // Magic number: FS_MAGIC
    uint32_t s_nblocks;     // Total number of blocks on disk
    struct File s_root;     // Root directory node
};
```
磁盘布局结构如图：

![图1](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab5/images/1.png?raw=true)

# 1.4 File Meta-data
JOS 文件系统使用 struct File 描述文件，该结构包含文件名，大小，类型，文件内容的block号。

JOS 文件系统采用混合索引：struct File 的 f_direct 数组保存前 NDIRECT（10）个block号，并分配一个额外的block来保存 4096/4=1024 个 block 号，所以允许文件最多拥有1034个block。

File结构如图：

![图2](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab5/images/2.png?raw=true)

File结构定义在inc/fs.h中：
```
struct File {
    char f_name[MAXNAMELEN];    // filename
    off_t f_size;           // file size in bytes
    uint32_t f_type;        // file type
    // Block pointers.
    // A block is allocated iff its value is != 0.
    uint32_t f_direct[NDIRECT]; // direct blocks
    uint32_t f_indirect;        // indirect block
    // Pad out to 256 bytes; must do arithmetic in case we're compiling
    // fsformat on a 64-bit machine.
    uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4];
} __attribute__((packed));  // required only on some 64-bit machines
```
# 1.5 Directories versus Regular Files
普通文件和目录文件由 File 结构的 type 字段区分。

超级块中包含一个 File 结构，代表文件系统的根目录文件。

# 2 The File System
这一部分仅需要补充文件系统的几处关键部件：读入 block 到 block cache / 写回 block 到磁盘、分配 block、计算文件偏移与 block 的映射、实现读写操作的 IPC 接口。

# 2.1 Disk Access
JOS 不像传统操作系统一样将磁盘驱动放在内核，而是将磁盘驱动作为一个文件系统进程的一部分，使得这个特殊的进程能够访问磁盘。（微内核？）

由于 IDE 磁盘寄存器位于 x86 的 I/O 空间，故需要给予文件系统进程一个 "I/O privilege" ：设置 EFLAGS 寄存器中的 IOPL 位，表示是否允许保护模式下代码执行设备IO指令，比如in和out。

>Exercise 1. 修改env_create()，如果type是ENV_TYPE_FS，需要给该进程IO权限。而文件系统进程的type为ENV_TYPE_FS。

注意文件 GNUmakefile 中设置 QEMU 来使用文件 obj/kern/kernel.img 作为 0 号磁盘的镜像（即DOS/Windows中的C盘），使用文件 obj/fs/fs.img 来作为 1 号磁盘的镜像（即D盘）。可以随时通过 `make clean` 加 `make` 来重置它们。

# 2.2 The Block Cache
这一部分借助虚存机制来实现一个简单的"buffer cache"（仅仅充当block cache），代码位于文件 fs/bc.c。

文件系统最大支持3GB，文件系统进程将0x10000000 (DISKMAP)到0xD0000000 (DISKMAP+DISKMAX)固定3GB的内存空间映射到磁盘空间，如block 0被映射到虚拟地址0x10000000，block 1被映射到虚拟地址0x10001000。函数 diskaddr 实现转换功能。

如果将整个磁盘全部读到内存将非常耗时，所以将实现按需加载：只有当访问某个bolck对应的内存地址时出现页错误，才将该block从磁盘加载到对应的内存区域，然后重新执行内存访问指令。

函数 bc_pgfault 负责在缺页异常时从磁盘载入页面。

函数 flush_block 负责将一个 block 写回磁盘，仅当需要写回时（即 PTE_D 位被设置）

>Exercise 2. 实现bc_pgfault()和flush_block()。

fs/fs.c中的fs_init()会将指向虚拟内存[DISKMAP, DISKMAP+DISKMAX]的指针存入全局结构 super 中，之后通过 super 变量就可以访问到磁盘((uint32_t)addr - DISKMAP) / BLKSIZE block中的数据。

# 2.3 The Block Bitmap
函数 fs_init 设置了 bitmap 指针，指向一个位数组，表示每个 block 使用情况。

函数 block_is_free 检查给定 block 是否被占用。

函数 alloc_block 搜索 bitmap 位数组，返回一个未使用的block号，并将其标记为已使用。

>Exercise 3. 实现 fs/fs.c中的 alloc_block 。

# 2.4 File Operations
文件 fs/fs.c 提供了一系列函数用于管理File结构、管理目录文件、解析绝对路径：
1. file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)：查找f指向文件结构的第filebno个block的地址，保存到ppdiskbno中。如果f->f_indirect还没有分配，且alloc为真，那么将分配一个新的block作为该文件的f->f_indirect。类似页表管理的pgdir_walk()。
2. file_get_block(struct File *f, uint32_t filebno, char **blk)：查找文件第filebno个block对应的虚拟地址addr，将其保存到blk地址。
3. walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)：解析路径path，填充pdir和pf。比如/aa/bb/cc.c，那么pdir指向代表bb目录的File结构，pf指向代表cc.c文件的File结构。又比如/aa/bb/cc.c，但是cc.c此时还不存在，那么pdir依旧指向代表bb目录的File结构，但是pf地址处应该为0，lastelem指向的字符串应该是cc.c。
4. dir_lookup(struct File *dir, const char *name, struct File **file)：查找dir指向的目录文件，寻找名为name的File结构，并保存到file。
5. dir_alloc_file(struct File *dir, struct File **file)：在dir目录文件的内容中寻找一个未被使用的File结构，保存到file。
6. file_create(const char *path, struct File **pf)：创建path，如果创建成功pf指向新创建的File。
7. file_open(const char *path, struct File **pf)：寻找path对应的File结构地址，保存到pf
8. file_read(struct File *f, void *buf, size_t count, off_t offset)：从文件f中的offset字节处读取count字节到buf处
9. file_write(struct File *f, const void *buf, size_t count, off_t offset)：将buf处的count字节写到文件f的offset处
    
>Exercise 4. 实现file_block_walk()和file_get_block()。

# 2.5 The file system interface
到目前为止，文件系统进程已经能提供各种操作文件的功能了，但是其他用户进程不能直接调用这些函数。

FS 进程通过进程间函数调用(remote procedure call / RPC)对其它进程提供文件系统服务。RPC 基于 IPC 机制：
```
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+

```

普通进程通过IPC向FS进程发送操作请求和操作数据，然后FS进程执行文件操作，最后又将结果通过IPC返回给普通进程。

客户端的代码在lib/fd.c和lib/file.c两个文件中:基于文件描述符的 read 调用合适的设备读函数 devfile_read（存在其他设备种类如 pipes，devfile_* 函数适合于磁盘上的文件），该函数将参数填入结构 union Fsipc 中以方便 IPC 请求、调用fsipc 、解析并返回结果。函数 fsipc 处理发送请求和接收结果的细节。 

服务端的代码在fs/fs.c和fs/serv.c两个文件中: serve 中有一个无限循环，接收IPC请求，将请求分配到对应的处理函数 serve_read，然后将结果通过IPC发送回去。 serve_read 处理 IPC 相关细节如解析参数，然后调用真正的操作执行函数 file_read。

客户端 IPC 发送参数：发送一个32位的值作为请求类型（预先编号），发送一个Fsipc结构作为请求参数，该数据结构通过IPC的页共享发给FS进程并映射到 fsreq(0x0ffff000)。

服务端 IPC 返回参数：大多数请求仅返回一个32位的值作为返回码，而FSREQ_READ 和 FSREQ_STAT 这两种请求还要将需返回的数据写入共享页面。

>Exercise 5. 实现 serve_read 。

>Exercise 6. 实现 serve_write 和 devfile_write 。

文件系统总结如图：

![图三](https://github.com/Crown798/MIT-JOS-2019/blob/master/Lab5/images/3.png?raw=true)

文件描述符层数据结构：
```
struct Fd {
	int fd_dev_id;
	off_t fd_offset;
	int fd_omode;
	union {
		// File server files
		struct FdFile fd_file;
	};
};

struct FdFile {
	int id;
};
```
设备层（file、pipe、console）：
```
struct Dev {
	int dev_id;
	const char *dev_name;
	ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len);
	ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len);
	int (*dev_close)(struct Fd *fd);
	int (*dev_stat)(struct Fd *fd, struct Stat *stat);
	int (*dev_trunc)(struct Fd *fd, off_t length);
};
```
文件层：
```
struct OpenFile {          // 用于联系 Fd 和 File 结构
	uint32_t o_fileid;	// file id
	struct File *o_file;	// mapped descriptor for open file
	int o_mode;		// open mode
	struct Fd *o_fd;	// Fd page
};

struct File {
	char f_name[MAXNAMELEN];	// filename
	off_t f_size;			// file size in bytes
	uint32_t f_type;		// file type

	// Block pointers.
	// A block is allocated iff its value is != 0.
	uint32_t f_direct[NDIRECT];	// direct blocks
	uint32_t f_indirect;		// indirect block

	// Pad out to 256 bytes; must do arithmetic in case we're compiling
	// fsformat on a 64-bit machine.
	uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4];
} __attribute__((packed));	// required only on some 64-bit machines
```
管道：
```
struct Pipe {
	off_t p_rpos;		// read position
	off_t p_wpos;		// write position
	uint8_t p_buf[PIPEBUFSIZ];	// data buffer
};
```

# 3 Spawning Processes
lib/spawn.c中的spawn()类似于UNIX中的fork()+exec()：创建一个新的进程，从文件系统加载用户程序，然后启动该进程。
具体逻辑如下：
1. 从文件系统打开命为prog的程序文件
2. 调用系统调用sys_exofork()创建一个新的Env结构
3. 调用系统调用sys_env_set_trapframe()，设置新的Env结构的Trapframe字段
4. 根据ELF文件中program herder，将用户程序的Segment读入内存，并映射到指定的线性地址
5. 调用系统调用sys_env_set_status()设置新的Env结构状态为ENV_RUNNABLE

>Exercise 7. 实现 kern/syscall.c 的 sys_env_set_trapframe。

# 3.1 Sharing library state across fork and spawn
UNIX文件描述符是一个大的概念，底层对应了多种设备如pipe，控制台I/O。在JOS中每种设备对应一个struct Dev结构，该结构包含函数指针，指向真正实现读写操作的函数。

lib/fd.c 定义了文件描述符结构 struct Fd 以及与文件描述符相关的函数，这些函数会将操作分发给合适的 struct Dev 中指向的函数。

lib/fd.c 维护了每个进程地址空间的文件描述符表区域，该区域大小为1页（够32个fd结构），起始于 FDTABLE（0xD0000000）。可以通过检查某个Fd结构的虚拟地址是否已经分配，来判断这个文件描述符是否被分配。如果一个文件描述符被分配了，那么该文件描述符对应的Fd结构开始的一页将被映射到和FS进程相同的物理地址处。

为了共享文件描述符，JOS中定义新的标志位PTE_SHARE，如果有个页表条目的PTE_SHARE标志位为1，那么这个PTE在fork()和spawn()中将被直接拷贝到子进程页表，从而让父进程和子进程共享相同的页映射关系。

>Exercise 8. 修改 lib/fork.c 的 duppage ，实现 lib/spawn.c 的 copy_shared_pages。

# 4 The Shell

# 4.1 The keyboard interface
首先需要使 shell 也能够接收键盘输入。

之前 kernel monitor 已使用了 kern/console.c 中的 keyboard and serial drivers 来处理两种输入：在QEMU中，图形界面的输入会成为JOS中的键盘输入， console 的输入为成为JOS中的 serial port 的输入。

>Exercise 9. 在 kern/trap.c 中调用 kbd_intr 和 serial_intr 来处理对应输入。

lib/console.c 中定义了 console 文件类型，它用于默认的标准输入输出文件。

# 4.2 The Shell
输入`make run-icode`，将会执行user/icode，user/icode又会执行inti，该函数将console设置为标准输入输出文件（0、1号文件描述符），然后spawn sh。然后就能运行如下指令：
```
    echo hello world | cat
    cat lorem |cat
    cat lorem |num
    cat lorem |num |num |num |num |num
    lsfd
```

用户库函数 cprintf 直接向 console 输出而不使用文件描述符，printf("...", ...) 默认使用1号文件描述符，fprintf(1, "...", ...)可以指定文件描述符。

>Exercise 10. 修改user/sh.c，使shell支持IO重定向。

