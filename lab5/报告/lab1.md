# <center>实验1</center>

## Part1 环境配置

![image-20260315220212669](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260315220212669.png)

使用Windows+wsl2完成作为实验环境

> 环境在之前的课程的已经配置，这里省去配置过程

## Part2 编译Linux内核——Linux6.6

```bash
cd ~/OScode/new_lab1/linux-6.6 

// 编译内核
make i386_defconfig

make menuconfig

make -j$(nproc)
```



![image-20260309103309405](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309103309405.png)

> 结果如上图所示，内核编译成功

## Part3 QEMU启动与调试

终端1：

在终端1中使用qemu启动内核并开启全程调试

```bash
qemu-system-i386 -kernel linux-5.10.19/arch/x86/boot/bzImage -s -S -append "console=ttyS0" -nographic
```

![image-20260309104029402](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309104029402.png)

![image-20260309104422971](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309104422971.png)

终端2：

在中断2中启动gdb

```bash
gdb

file linux-6.6/vmlinux

target remote:1234

break start_kernel

c
```



![image-20260309104404495](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309104404495.png)

![image-20260316082349931](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316082349931.png)

在执行到断点后qemu中输出如上图所示。

### 思考题

1.`start_kernel`函数位于哪个源文件中？提示：用`info`或`list`。

![image-20260316083052799](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316083052799.png)

尝试使用`info line start_kernel`命令，gdb返回了`No line number information available for address`并给出了`start_kernel`函数的地址。这表明了系统虽然解析出了函数地址，但无法映射到源文件行号。

可能得原因是默认的内核配置文件(i386_deconfig)为了优化编译和减小体积，没有开启config_debug_info，所以无法映射到行号。



2.使用`info registers`查看寄存器状态。在i386模式下，你能看到哪些寄存器（EAX、EBX、ESP等）？

![image-20260316081719508](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316081719508.png)

可以查看到的寄存器如上图所示。其中第一列是寄存器名称，第二列是寄存器中的十六进制原始值，第三列是gdb提供的辅助解析值。



## Part4 Initramfs制作

```
cd ~/new_lab1
```

在`new_lab1`构建helloworld.c文件。

```c
#include <stdio.h>

void main()
{
    printf("Hello World\n");
    fflush(stdout);
    /* 让程序打印完后继续维持在用户态 */
    while(1);
}
```

helloworld.c文件内容如上。

```
gcc -o helloworld -m32 -static helloworld.c
```

静态编译32位可执行文件。

```
echo helloworld | cpio -o --format=newc > hwinitramfs
```

用cpio打包initramfs.

```
qemu-system-i386 -kernel linux-6.6/arch/x86/boot/bzImage -initrd initramfs.cpio
```

启动内核，并加载initramfs。

![image-20260309105004639](D:\桌面\image-20260309105004639.png)

![image-20260316080356458](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316080356458.png)

QEMU中输出`Hello World` 如图所示。窗口维持在此界面。

## Part5 编译并启动Busybox 

下载并编译Busybox

![image-20260309105739014](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309105739014.png)

搭建根文件系统目录树

![image-20260309105937058](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309105937058.png)

创建设备文件

![image-20260309110031083](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309110031083.png)

编写init

![image-20260309110324850](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309110324850.png)

打包 Initramfs

![image-20260309110439724](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309110439724.png)

进入Busybox Shell

![image-20260309113219435](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309113219435.png)

查看信息

```
uname -a
```

![image-20260316085330175](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316085330175.png)

```
cat /proc/cpuinfo
```

![image-20260316085413749](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316085413749.png)

```
cat proc/meminfo
```

![image-20260316085448084](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316085448084.png)

```
ps
```

![image-20260316085509260](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316085509260.png)

### 思考题

1.尝试在`start_kernel`中单步执行几步（`n`），观察内核初始化的前几个函数调用。

终端1：

```
cd ~/OScode/new_lab1

qemu-system-i386 -kernel linux-6.6/arch/x86/boot/bzImage -initrd initramfs-busybox.cpio.gz -nographic -append "nokaslr console=ttyS0" -m size=2048 -s -S
```



![image-20260316090556609](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316090556609.png)

![image-20260316090618279](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316090618279.png)

终端2：

```bash
cd ~/OScode/new_lab1/linux-6.6

gdb vmlinux

//gdb中

target remote :1234 // 连接终端1中的QEMU

break start_kernel // 打入断电

c // continue

n // next
```



![image-20260316090514878](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316090514878.png)

之后在终端2的命令行中敲击空格键可以看到一下输出

![image-20260316091335865](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316091335865.png)

如上图所示，即为内核初始化时的函数调用。输出给出了函数的内存地址但是无法给出行号，推测原因和之前相同。



## Part6 Linux0.11内核的编译、启动和调试

### 解压并编译

![image-20260309114450160](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309114450160.png)



### gdb 调试

终端1：

启动QEMU并挂起

```bash
cd ~/OScode/new_lab1/Linux-0.11
qemu-system-i386 -m 16 -boot a -fda Image -hda hdc-0.11.img -s -S
```

终端2：

启动 GDB 并在关键位置打下断点

```
cd ~/OScode/new_lab1/Linux-0.11
gdb tools/system
```

```
// 在gdb中
(gdb) target remote :1234
(gdb) set disassembly-flavor intel
(gdb) b *0x7c00
(gdb) b main
(gdb) c
```



![image-20260309120822820](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260309120822820.png)

当程序停在 `0x7c00` 时，系统其实刚好加载了启动扇区。按照规定，启动扇区的最后两个字节（地址 `0x7DFE` 和 `0x7DFF`）必须是 `0x55` 和 `0xAA`，在右图中的输出可以看到，成功输出了`0x55` 和 `0xAA`。



### 挂载minix文件系统

由于WSL Ubuntu内核不支持Minix，这里使用Part2中已经编译的Linux6.6内核，修改其.config，设置config_minix_fs=y，将Minix文件系统的驱动代码直接静态编译到新的Linux6.6内核镜像中。

1.使用启动脚本

```bash
cd ~/OScode/new_lab1/busybox_rootfs

cat << 'EOF' > init
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys

echo -e "\n========================================="
echo -e "  Matrix's Auto-Patcher is running...    "
echo -e "========================================="

#  捏出硬盘分区节点
mknod /dev/sda1 b 8 1
#  创建挂载点
mkdir -p /mnt
#  挂载 Minix 硬盘
mount -t minix /dev/sda1 /mnt

#  写入文件
echo "Auto-patched from 2026! Matrix OS is invincible." > /mnt/usr/hello.txt

#  暴力刷盘并卸载
sync
sync
umount /mnt

echo -e "  Patch Complete! You can exit now.      "
echo -e "=========================================\n"

exec /bin/sh
EOF

chmod +x init
```

> 此处脚本文件代码参考自大语言模型

2.打包镜像

```bash
find . | cpio -o -H newc | gzip > ../initramfs-busybox.cpio.gz
```

```bash
cd ~/OScode/new_lab1
qemu-system-i386 -kernel linux-6.6/arch/x86/boot/bzImage -initrd initramfs-busybox.cpio.gz -hda Linux-0.11/hdc-0.11.img
```

3.检查结果

```bash
cd ~/OScode/new_lab1/Linux-0.11
qemu-system-i386 -m 16 -boot a -fda Image -hda hdc-0.11.img
```

```
cat /usr/hello.txt
```

![image-20260316093538324](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316093538324.png)

可以看到脚本文件自动创建了hello.txt

![image-20260316093036198](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316093036198.png)

这里可以成功查看到脚本文件写入的hello.txt文件。

## Part7 认识麒麟操作系统

> 由于无法安装麒麟OS，接下来的两个part依旧使用WSL Ubuntu

### 了解基本信息

查看操作系统版本：

```
cat /etc/os-release 
```

![image-20260316164134142](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164134142.png)

查看内核版本和CPU架构：

```
uname -a
```

![image-20260316164248702](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164248702.png)

查看CPU信息：

```
cat /proc/cpuinfo
```

![image-20260316164444290](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164444290.png)

查看内存和磁盘信息：

```
free -h
df -h
```

![image-20260316164536550](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164536550.png)

![image-20260316164557433](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164557433.png)



### 包管理器对比

尝试使用apt搜索和查看一些软件包：

```
apt search gcc

apt show gcc

dpkg -l | grep gcc
```

![image-20260316164845886](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164845886.png)

![image-20260316164913125](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316164913125.png)

![image-20260316165312749](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316165312749.png)

### 了解CPU架构

```
# 查看CPU架构
uname -m

# 查看CPU详细信息
lscpu

# 查看GCC默认目标架构
gcc -dumpmachine
```

![image-20260316165351352](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316165351352.png)

![image-20260316165413107](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316165413107.png)

![image-20260316165446354](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316165446354.png)



### 思考题

1.为什么ARM架构在手机和嵌入式领域占据主导地位，而x86在桌面和服务器领域更为流行？RISC与CISC两种设计哲学各有什么优缺点？

​	ARM：**更高的能效比**，移动端设备主要依靠电池供电，没有主动散热（风扇）的条件。ARM 基于 RISC 架构，设计精简，具有功耗极低、发热量小的天生优势。**灵活的商业模式**，ARM公司本身不做芯片，而是出售架构，使得其他公司可以基于ARM的指令集制作芯片。

​	x86：**追求更高性能**，电脑桌面和数据中心服务器通常连接着稳定的电源，拥有庞大的散热系统，可以给出更高的性能输出。**拥有生态护城河**，x86 拥有近 40 年的历史包袱和极强的向下兼容性。全球绝大多数的 PC 专业软件、大型游戏以及老旧的企业级服务器后台，都是基于 x86 指令集编译的。

​	RISC：其设计哲学有*简化的指令集*、*高效的流水线*、*Load/Store 架构*、*优化常用指令*。优点有通用*寄存器多、低功耗、高执行速度、硬件控制精简*。缺点有*代码体积大、复杂计算能力弱、带宽要求高*。

​	CISC：其设计哲学有*硬件复杂化，软件简化、兼容性优先、多种寻址模式*。其优点有*代码紧凑、强大的生态与兼容性、编译设计简单*。缺点有*能效比较低、流水线优化困难、硬件实现复杂。*



## Part8 探索麒麟OS内核

### 探索/proc虚拟文件系统

查看内核版本：

```
cat /proc/version
```

![image-20260316170908801](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316170908801.png)

查看系统运行时间：

```
cat /proc/uptime
```

![image-20260316170952878](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316170952878.png)

查看内存使用详情：

```
cat /proc/meminfo
```

![image-20260316171052275](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316171052275.png)

查看当前进程的信息：

```
cat /proc/self/status
```

![image-20260316171147651](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316171147651.png)

查看系统支持的文件系统：

```
cat /proc/filesystems
```

![image-20260316171251925](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316171251925.png)

查看内核启动命令行参数：

```
cat /proc/cmdline
```

![image-20260316171338988](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316171338988.png)

### 分析系统启动日志

查看内核启动前50行日志：

```
sudo dmesg | head -50
```

![image-20260316172540119](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316172540119.png)

查看与启动相关的信息：

```
sudo dmesg | grep -i "boot"
```

![image-20260316172654895](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316172654895.png)

查看CPU初始化信息

```
sudo dmesg | grep -i "cpu"
```

![image-20260316172924879](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316172924879.png)

查看内存初始化信息

```
sudo dmesg | grep -i "memory"
```

![image-20260316172952273](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316172952273.png)

查看设备驱动加载信息

```
sudo dmesg | grep -i "driver"
```

![image-20260316173014854](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316173014854.png)

### 理解内核模式



```
lsmod // 查看已加载的内核模块
modinfo ext4 // 查看某个模块的详细信息
```

![image-20260316173754280](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316173754280.png)

创建工作目录

```bash
mkdir -p ~/lab1/mymodule
cd ~/lab1/mymodule
```

创建内核模块源文件`hello_module.c`

```c
cat << 'EOF' > hello_module.c
/* hello_module.c - 一个简单的Hello World内核模块 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("A simple Hello World kernel module for Ubuntu");
MODULE_VERSION("1.0");

/* 模块加载时执行的函数 (装载 U 盘) */
static int __init hello_init(void){
    // 注意：内核空间不能用 printf，必须用内核专属的 printk
    printk(KERN_INFO "Hello! This module is successfully running on Ubuntu.\n");
    return 0;
}

/* 模块卸载时执行的函数 (拔出 U 盘) */
static void __exit hello_exit(void){
    printk(KERN_INFO "Goodbye! Module unloaded from Ubuntu.\n");
}

module_init(hello_init);
module_exit(hello_exit);
EOF
```

创建Makefile

```shell
cat << 'EOF' > Makefile
obj-m += hello_module.o
# 关键修改：不要用 $(shell uname -r)，直接指向你编译好的 Linux 6.6 目录！
KDIR := /home/matrix/OScode/new_lab1/linux-6.6
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
EOF
```

> WSL Ubuntu不支持源码头文件(Headers)，使用Linux6.6进行模块编译，后使用QEMU模拟器中运行。

编译模块

```sh
make
```

将模块打包进Linux6.6

```
# 把编译好的内核模块复制到你的极简文件系统目录中
cp hello_module.ko ~/OScode/new_lab1/busybox_rootfs/

# 进入目录并重新打包成 cpio 镜像
cd ~/OScode/new_lab1/busybox_rootfs
find . | cpio -o -H newc | gzip > ../initramfs-busybox.cpio.gz
```

启动QEMU并进行热插拔测试

```
cd ~/OScode/new_lab1
qemu-system-i386 -kernel linux-6.6/arch/x86/boot/bzImage -initrd initramfs-busybox.cpio.gz
```

```
# 挂载模块
insmod /hello_module.ko

# 查看内核底层日志（Busybox 里没有 journalctl，直接用 dmesg）
dmesg | tail -n 5
# （这里你应该能看到 Hello! This module is successfully running on Ubuntu.）

# 查看已经加载的模块
lsmod

# 拔出模块
rmmod hello_module

# 再次查看内核日志确认告别语
dmesg | tail -n 3
```

![image-20260316175018402](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316175018402.png)

可以看到在QUME中给出了预期的输出。

### 观察系统调用

观察 `ls`命令的体统调用

```
strace ls /tmp 2>&1 | head -30
```

![image-20260316175318503](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316175318503.png)

观察`cat`命令读取文件时的系统调用：

```
strace cat /proc/version 2>&1 | head -30
```

![image-20260316175413282](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316175413282.png)

统计一个程序使用的系统调用种类和次数：

```
strace -c ls /tmp
```

![image-20260316175439450](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316175439450.png)

### 使用QEMU体验跨架构运行

查看QEMU支持的架构：

```
ls /usr/bin/qemu-system-*
```

![image-20260316175710418](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316175710418.png)

安装ARM64的QEMU

```
sudo apt install qemu-system-aarch64
```

用QEMU启用一个ARM64系统

```
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 512 -kernel linux-6.6/arch/arm64/boot/Image -nographic -append "console=ttyAMA0"
```

![image-20260316181617327](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260316181617327.png)

在QEMU非图形化界面中成功启动了ARM64内核，但由于系统中没有编译ARM64的Initramfs所以在启动之后发生了Kernel panic。
