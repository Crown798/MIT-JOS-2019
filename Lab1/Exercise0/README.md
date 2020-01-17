# 1 环境准备
## 1.1 虚拟机
### 1.1.1 虚拟机配置
- VirtualBox
- Ubuntu-16.04.6-desktop-i386
    - [官网镜像](http://releases.ubuntu.com/16.04/)
- 阿里软件源

### 1.1.2 配置步骤参考
    https://www.linuxidc.com/Linux/2016-08/134580.htm
    https://www.cnblogs.com/luengmingbiao/p/10859905.html
    https://blog.csdn.net/weixin_34129696/article/details/91447944

## 1.2 课程Lab用到的工具
https://pdos.csail.mit.edu/6.828/2018/tools.html

### 1.2.1 编译工具链
编译工具链包括：c编译器、汇编器、链接器、调试器，用以编译测试kernel

输入以下命令，控制台输出`elf32-i386`
```
% objdump -i
```
输入以下命令，控制台输出`/usr/lib/gcc/i486-linux-gnu/version/libgcc.a` or `/usr/lib/gcc/x86_64-linux-gnu/version/32/libgcc.a`
```
% gcc -m32 -print-libgcc-file-name
```
若以上输出不正确，需要 install a development environment
```
% sudo apt-get install -y build-essential gdb
```
在64位的机器上还需要安装32位支持库
```
% sudo apt-get install gcc-multilib
```
### 1.2.2 x86模拟器
QEMU，用以运行kernel

clone源码
```
git clone https://github.com/mit-pdos/6.828-qemu.git qemu
```
install several libraries:libsdl1.2-dev, libtool-bin, libglib2.0-dev, libz-dev, and libpixman-1-dev.
```
sudo apt-get install 'pkg-names'
```
Configure the source code (optional arguments are shown in square brackets; replace PFX with a path of your choice)
```
./configure --disable-kvm --disable-werror [--prefix=PFX] [--target-list="i386-softmmu x86_64-softmmu"]
```
执行安装
```
make && make install
```
### 1.2.3 编译运行kernel
源码下载
```
git clone https://pdos.csail.mit.edu/6.828/2018/jos.git lab
```
编译：在lab源码根目录下执行`make`

启动：在lab源码根目录下执行`make qemu`或者`make qemu-nox`（use the serial console without the virtual VGA）

调试：需要开两个终端，一个以调试模式启动qemu，启动后qemu不会立即开始运行指令，而是等待gdb连接后通过gdb控制qemu的运行
```
make qemu-gdb
make gdb
```
终止qemu运行，按下`Ctrl+a x`

评分：自动编译内核，运行，并查看是否满足Lab要求
```
make grade
```
### 1.2.4 编译时遇到的问题
>/usr/bin/python^M: 解释器错误: 没有那个文件或目录

需要转换脚本文件格式为unix
https://blog.csdn.net/sjyu_ustc/article/details/76582993
https://www.cnblogs.com/Braveliu/p/10560429.html

>ln: 无法创建符号链接“”: 不允许的操作

编译的源码放在linux虚拟机和windows系统的共享目录下，共享目录所在的文件系统和linux的文件系统并不是同一个文件系统，故而不能创建硬链接
https://blog.csdn.net/daze_scarecrow/article/details/78831789

>使用qemu启动内核时出现 qemu-system-i386  Could not initialize SDL(No available video device) - exiting

因为qemu默认需要使用SDL方式在图形界面中使用，若使用ssh登录虚拟机则无图形界面，会报错
解决办法：打开图形界面即可；使用make qemu-nox命令，设置不使用VGA界面
http://smilejay.com/2012/08/kvm-sdl-display/