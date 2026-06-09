# lab7

## Assignment1 

### 1.1

复现 `src/3` 的物理页内存管理代码

![image-20260608111343819](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608111343819.png)

在 `first_thread` 中编写测试代码：

1.分别从内核物理地址池和用户物理地址池中分配若干页（如 10 页、20 页、50 页），打印分配的起始地址。

![image-20260608113246132](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608113246132.png)

2.释放部分页后再次分配，验证释放的空间是否可被复用。

![image-20260608113346495](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608113346495.png)

3.截图展示运行结果，并***\结合代码逐步分析\*** BitMap → AddressPool → MemoryManager 三层管理结构的初始化过程和分配/释放流程。



### 1.2

复现 `src/4` 的代码，开启二级分页机制

![image-20260608200319966](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608200319966.png)

1.结合代码分析开启分页机制的三步方案：(a) 初始化页目录表和页表，(b) 将页目录表地址写入 CR3，(c) 将 CR0 的 PG 位置 1。



2.说明 `directory[0]`、`directory[768]`、`directory[1023]` 三个页目录项各自的作用以及为什么要这样设置。



3.使用 QEMU Monitor 的 `info mem` 命令，截图展示开启分页机制后的虚拟地址映射关系，验证 0~1MB 的恒等映射是否正确建立。

![image-20260608213311921](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608213311921.png)





## Assignment2

### 2.1



```
=== screen ===
VGA Blank mode
SeaBIOS (version 1.15.0-1)
iPXE (https://ipxe.org) 00:03.0 CA00 PCI2.10 PnP PMM+07F8B590+07ECB590 CA00
Booting from Hard Disk...
open page mechanism
total memory: 133038080 bytes ( 126 MB )
kernel pool
start address: 0x200000
total pages: 15984 ( 62 MB )
bitmap start address: 0x10000
user pool
start address: 0x4070000
bit map start address: 0x107CE
kernel virtual pool
start address: 0xC0100000
total pages: 15984  ( 62 MB )
bit map start address: 0x10F9C
[First Fit] alloc 8 -> idx=0 |   [Used=23 MaxFree=15919 Frag=3]
[First Fit] cleanup |   [Used=0 MaxFree=15984 Frag=1]
[Best Fit] alloc: 30@0 10@30 20@40 5@60 |   [Used=65 MaxFree=15919 Frag=1]
[Best Fit] free A(30) C(20) -> holes:30 & 20 |   [Used=15 MaxFree=15919 Frag=3]
[Best Fit] alloc 8 -> idx=40 |   [Used=23 MaxFree=15919 Frag=3]
[Best Fit] cleanup |   [Used=0 MaxFree=15984 Frag=1]
[Worst Fit] alloc: 30@0 10@30 20@40 5@60 |   [Used=65 MaxFree=15919 Frag=1]
[Worst Fit] free A(30) C(20) -> holes:30 & 20 |   [Used=15 MaxFree=15919 Frag=3]
[Worst Fit] alloc 8 -> idx=65 |   [Used=23 MaxFree=15911 Frag=3]
[Worst Fit] cleanup |   [Used=0 MaxFree=15984 Frag=1]
[Next Fit] alloc: 30@0 10@30 20@40 5@60 |   [Used=65 MaxFree=15919 Frag=1]
[Next Fit] free A(30) C(20) -> holes:30 & 20 |   [Used=15 MaxFree=15919 Frag=3]
[Next Fit] alloc 8 -> idx=65 |   [Used=23 MaxFree=15911 Frag=3]
[Next Fit] cleanup |   [Used=0 MaxFree=15984 Frag=1]
========== FINAL SUMMARY ==========
 Strategy  | alloc(8) idx | Expected
 ----------|--------------|---------
 First Fit | 0             | 0  (first)
 Best Fit  | 40             | 40 (smallest)
 Worst Fit | 65             | 65 (largest)
 Next Fit  | 65
| 65 (lastIdx)
===================================

```





### 2.2 



```
=== screen ===
VGA Blank mode
SeaBIOS (version 1.15.0-1)
iPXE (https://ipxe.org) 00:03.0 CA00 PCI2.10 PnP PMM+07F8B590+07ECB590 CA00
Booting from Hard Disk...
open page mechanism
mem: 126 MB, kernel: 15984 pages, user: 15984 pages
===== Lab 2.2: Memory Utilization =====
10 alloc + 5 free: First Fit vs Best Fit
[FF] =====
[FF] Step   1  2  3  4  5  6  7  8  9 10  11 12 13 14 15
[FF] Op   +20 +10 +30  +5  -20 +15 -30  +8 -10 +12  -5  +7 +25 -15  +3
[FF] Used  20 30 60 65 45 60 30 38 28 40  35 42 67 52 55
[FF] Frag  1 1 1 1 2 2 3 2 3 2  1 1 1 2 2
[FF] MaxFree15964,15954,15924,15919,15919,15919,15919,15919,15919,15919, 15949,1
5942,15917,15917,15917
[BF] =====
[BF] Step   1  2  3  4  5  6  7  8  9 10  11 12 13 14 15
[BF] Op   +20 +10 +30  +5  -20 +15 -30  +8 -10 +12  -5  +7 +25 -15  +3
[BF] Used  20 30 60 65 45 60 30 38 28 40  35 42 67 52 55
[BF] Frag  1 1 1 1 2 2 3 2 3 2  1 1 1 2 2
[BF] MaxFree15964,15954,15924,15919,15919,15919,15919,15919,15919,15919, 15949,1
========== Summary ==========
First Fit: first hole | quick
Best Fit:  smallest hole | saves big blocks
=============================

```







## Assignment3

### 3.1

复现 `src/5` 的代码

![image-20260608215635250](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608215635250.png)



1. 结合代码分析虚拟页内存分配的三步过程：(a) 从虚拟地址池分配连续虚拟页，(b) 从物理地址池为每个虚拟页分配物理页，(c) 建立页目录项和页表项。






2. 在 `first_thread` 中分配多组虚拟页（如 100 页、10 页、100 页），打印虚拟地址；释放中间的 10 页后重新分配，观察地址变化。

```
open page mechanism
[MEM] total 126MB | kernel phys: 15984 pages (0x200000) | kernel virt: 15984 pages (0xC0100000)
=== 3.1 Alloc/Release Test ===
#1 +100 v=C0100000 PDE=FFFFFC00 PTE=FFF00400 ph=200000
#2  +10 v=C0164000 PDE=FFFFFC00 PTE=FFF00590 ph=264000
#3 +100 v=C016E000 PDE=FFFFFC00 PTE=FFF005B8 ph=26E000
#4  -10 @C0164000 (released)
#5 +100 v=C01D2000 PDE=FFFFFC00 PTE=FFF00748 ph=264000
#6  +10 v=C0164000 PDE=FFFFFC00 PTE=FFF00590 ph=32C000
=== Done ===

```





### 3.2

1. **假设**将指向页目录表本身的页目录项改为第 **1000** 个（而不是第 1023 个），请推导：
   - 第 **141** 个页目录项的虚拟地址
   - 第 **891** 个页目录项指向的页表中，第 **109** 个页表项的虚拟地址



2. 写出你的推导过程和最终结果（十六进制）。





### 3.3





```
open page mechanism
[MEM] total 126MB | kernel phys: 15984 pages (0x200000) | kernel virt: 15984 pages (0xC0100000)
=== 3.3 Vaddr to Paddr Verification ===
#1 Allocated vaddr: 0xC0100000
#2 vaddr2paddr -> phy: 0x200000
#3 Wrote 0xDEADBEEF to *(C0100000)=DEADBEEF
    (verify via QEMU Monitor: xp 0x200000)
=== Done (looping for monitor) ===

--- QEMU Monitor xp verification ---
0000000000200000: 0xdeadbeef

```

![image-20260609173028152](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260609173028152.png)
