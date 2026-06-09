# <center>Lab3 从实模式到保护模式</center>

**本次实验部分代码和注释参考自大模型，其中Assignment4中的代码由大模型完成。**

> 实验环境：Ubuntu22.04

## Assignment1 磁盘加载

### 1.1 复现Example1

这里使用提供的Makefile来自动处理汇编、制作镜像和启动模拟器。

```shell
make build

make run
```

![image-20260411184747825](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411184747825.png)

![image-20260411184810639](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411184810639.png)

可以看到QEMU上成功显示 `run bootloader` 的青色字符。

### 1.2 CHS方式启动硬盘

**LBA → CHS 转换公式：**

对于标准硬盘，通常每磁道扇区数 $\text{SPT}=63$，每柱面磁头数 $\text{HPC}=18$。

- 柱面号 $C = \lfloor LBA \div (SPT \times HPC) \rfloor$
- 磁头号 $H = \lfloor LBA \div SPT \rfloor \mod HPC$
- 扇区号 $S = (LBA \mod SPT) + 1$

其中 SPT = 每磁道扇区数，HPC = 每柱面磁头数。注意 CHS 的扇区号从1开始，而LBA从0开始。

**实现思路：**

使用 `int 13h` 中断，由于这里要读取 LBA = 1 到 LBA = 5 ，可以使用一个通用的转换和读取子程序。

```assembly
; read_sector_chs: 使用 int 13h 读取一个扇区
; 参数:
;   ax = LBA 逻辑扇区号
;   es:bx = 目标内存地址

read_sector_chs:
    pusha                ; 保护所有通用寄存器

    ; 计算 S = (LBA % 63) + 1
    mov cl, 63           ; SPT = 63
    div cl               ; AL = LBA / 63 (商), AH = LBA % 63 (余数)
    mov cl, ah           
    inc cl               ; CL = S (扇区号)

    ; 计算 C 和 H
    ; 此时 AL = LBA / 63, 我们需要再除以 HPC (18)
    mov ah, 0            ; 清空 AH 准备下一次除法
    mov dl, 18           ; HPC = 18
    div dl               ; AL = (LBA/63) / 18 -> C (柱面号)
                         ; AH = (LBA/63) % 18 -> H (磁头号)
    mov ch, al           ; CH = C
    mov dh, ah           ; DH = H

    ; 调用 int 13h
    mov dl, 0x80         ; 驱动器号 80h (第一块硬盘)
    mov ah, 0x02         ; 功能号 02h: 读扇区
    mov al, 0x01         ; 读 1 个扇区
                         ; BX 已经是要写入的内存偏移地址
    int 0x13             ; 调用 BIOS 中断

    popa                 ; 恢复寄存器
    ret
```



这里使用提供的Makefile来自动处理汇编、制作镜像和启动模拟器。

```shell
make build

make run	
```

![image-20260411190940994](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411190940994.png)

![image-20260411191004250](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411191004250.png)

可以看到，在使用CHS方式启动磁盘后，QEMU上成功显示 `run bootloader` 的青色字符。



### 1.3 加载校验

**实现思路：**

为了确保 Bootloader 没有被损坏且被完全加载，在 Bootloader 的最末尾写入一个约定的标志 `0xCAFEBABE`。MBR 读完硬盘后，去该内存位置比对，如果相同，说明加载成功，否则打印报错信息并死机。

bootloader 限定为 5 个扇区（$5 \times 512 = 2560$ 字节，即 `0xA00`）。加载地址在 `0x7E00`。 魔数的预期物理地址 = $0\text{x}7\text{E}00 + 0\text{xA}00 - 4 = 0\text{x}87\text{FC}$。

这里使用提供的Makefile来自动处理汇编、制作镜像和启动模拟器。

```sh
make build 

make run
```



**跳转失败：**

当修改魔数为不为0xCAFEBABE，证明bootloader运行失败。

![image-20260411193601494](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411193601494.png)

可以看到QEMU中成功输出了`BOOT ERR`。

**跳转成功：**

当魔数为0xCAFEBABE，证明bootloadrt运行成功。

![image-20260411194448786](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260411194448786.png)

可以看到QEMU中成功输出了`BOOT OK`。



## Assignment 2 保护模式分析

### 2.1 调试保护模式——复现 Example2

使用Makefile进行编译

```sh
make build
make symbol
make debug
```

![image-20260412165928668](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412165928668.png)



在`0x7c00`处设置断点并执行。

![image-20260412170404131](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412170404131.png)

之后使用si、ni命令进行单步调试，代码执行到使用 `lgdt` 指令加载全局描述符表。

![image-20260412171418669](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412171418669.png)

打开a20地址线，设置cr0的PE位。

![image-20260412171808059](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412171808059.png)

可以看到cr0的最低位是1，说明已经开启保护模式。

![image-20260412171933091](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412171933091.png)

跳转进入保护模式：可以看到此时cs寄存器的值已经是0x20了，与boot.inc中的定义一致。

![image-20260412172115641](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412172115641.png)

之后让程序全速运行，运行结束后查看`GDT`的5个段描述符的内容。可以看到GDT的内容与设置相吻合。

![image-20260412172513495](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412172513495.png)

### 2.2 手工解析段描述符

第二个描述符是数据段，GDT偏移 `0x08`。

在 GDB 中输入 `x /2xw 0x8808` 得到如下十六进制：

![image-20260412174857125](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412174857125.png)

高4字节：0x00cf9200

低4字节：0x0000ffff

| **字段**            | **位区间**       | **解析含义**     |
| ------------------- | ---------------- | ---------------- |
| **段界限 (Limit)**  | 低16位 + 高4位   | $20$ 位全 1      |
| **段基地址 (Base)** | 多个区间拼接     | $32$ 位全 0      |
| **G (粒度)**        | 高4字节第 23 位  | 长度单位为 $4KB$ |
| **D/B**             | 高4字节第 22 位  | $32$ 位操作数    |
| **S**               | 高4字节第 12 位  | 存储器段         |
| **TYPE**            | 高4字节 8-11 位  | 数据段：可读写   |
| **DPL**             | 高4字节 13-14 位 | 特权级 0         |
| **P**               | 高4字节第 15 位  | 段在内存中存在   |

可以看到由于低位字节存储在低地址中，所以是小端存储。

### 2.3 自定义段描述符

**实现思路：**在全局描述符表（GDT）中新增一个自定义数据段，设定其基地址为 `0x7000`，段界限为 `0x1FFF`。将该段对应的选择子（0x28）加载到 `FS` 寄存器中。利用 `FS:0x0` 将学号字符串写入内存。此时物理地址 = `0x7000 (FS基址) + 0x0 (偏移) = 0x7000`。利用 `DS:0x7000` 将刚刚写入的数据读出并显示到屏幕上。

构造自定义段描述符：

```assembly
; 基地址=0x7000, 界限=0x1FFF, 粒度=字节, DPL=0
mov dword [GDT_START_ADDRESS+0x28], 0x70001fff
mov dword [GDT_START_ADDRESS+0x2c], 0x00409200
```

通过 FS 段写入数据：

```assembly
	mov ax, 0x28           ; 选择子 0x28 指向刚才创建的第 6 个段
    mov fs, ax             ; 将选择子装入 FS 寄存器

    mov esi, my_student_id
    mov edi, 0             ; 偏移量设为 0
    mov ecx, 8             ; 循环 8 次（学号有 8 位）
.write_to_fs:
    mov al, [esi]
    mov [fs:edi], al       ; 核心：向 FS:edi 写入单字符
    inc esi
    inc edi
    loop .write_to_fs
```

通过 DS 段读取数据：

```assembly
mov esi, 0x7000        ; 偏移量设为 0x7000
    mov edi, 80 * 2 * 3    ; 显存地址偏移（控制显示在屏幕第4行）
    mov ecx, 8             ; 循环 8 次
    mov ah, 0x0E           ; 设置颜色为黄色
.read_from_ds:
    mov al, [ds:esi]       ; 核心：从平坦模式的 DS:esi 读取数据
    mov word [gs:edi], ax  ; 写入视频段，显示到屏幕上
    inc esi
    add edi, 2
    loop .read_from_ds
```

使用Makefile进行编译。

```sh
make build

make run
```

可以看到屏幕上成功显示出了学号。证明通过DS段成功读取到了FS段写入的数据。

![image-20260412180304168](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260412180304168.png)



## Assignment3 32位保护编程模式

### 3.1 移植 Lab2 程序到保护模式

在32位保护模式下，BIOS中断失效无法使用`int 0x10`（清屏）这里使用手动循环写现存空格，对于`int 0x15`（延时）使用`dec ecx / jnz` 忙等待循环来处理，对于实模式下的16位寄存器这里变量改为dd，寄存器使用`eax/edi` 等。

主循环代码如下：

```assembly
main_loop:
    ; 计算显存偏移 = (y×80 + x) × 2
    mov  eax, [var_y]
    mov  ecx, 80
    mul  ecx
    add  eax, [var_x]
    shl  eax, 1              ; ×2（每格占2字节）
    mov  edi, eax

    mov  al, [var_char]
    mov  ah, [var_color]
    mov  [gs:edi], ax        ; 写入视频段

    call delay_pm            ; 忙等待延时

    inc  byte [var_color]    ; 循环变色
    inc  byte [var_char]     ; 循环变字符 '0'~'9'
    cmp  byte [var_char], '9' + 1
    jne  .check_dir
    mov  byte [var_char], '0'
    ; ... 边缘移动逻辑（与 Lab2 相同，改用 dword 变量）...
    jmp  main_loop
```



### 3.2 实现 32 位十六进制内存转储函数

查表将字节转为16进制，hex_dump的核心算法如下：
```assembly
hex_table db '0123456789ABCDEF'

; 设 AL = 待转换字节（例如 0xB8）

; 取高4位 nibble（= 0xB = 11）
mov  ah, al
shr  ah, 4           ; ah = 0x0B
and  ah, 0x0F        ; 确保只有低4位（防止符号扩展影响）
movzx eax, ah        ; 零扩展为32位，用作查表下标
mov  al, [hex_table + eax]  ; al = 'B'（ASCII 0x42）

; 取低4位 nibble（= 0x8 = 8）
mov  al, [esi]       ; 重新读取原字节
and  al, 0x0F        ; al = 0x08
movzx eax, al
mov  al, [hex_table + eax]  ; al = '8'（ASCII 0x38）
```



### 3.1 & 3.2 实验结果

Assignment3的整体实验架构沿用Example2，只对其中的`bootloader.asm`进行改动。其大致执行顺序如下：
````assembly
上电 → BIOS 加载 MBR(0x7C00)
  ↓
mbr.asm [bits 16]：读磁盘扇区1~5 → 加载 bootloader 到 0x7E00 → jmp 0x7E00
  ↓
bootloader [bits 16]：
  1. 实模式打印 'run bootloader'（纯16位寄存器，避免 0x66 操作数前缀）
  2. 向 0x8800 写入5个 GDT 描述符
  3. LGDT 加载 GDTR，开 A20，置 CR0.PE，远跳切入32位
  ↓
bootloader [bits 32]：
  4. 加载各段选择子到 DS/ES/SS/GS
  5. Task 3.2：clear_screen → 打印标题 → hex_dump(0x8800, 40字节) → 等待4秒
  6. Task 3.1：clear_screen → 绘制中央文字 → 跑马灯主循环
````



使用Makefile进行编译：

```sh
make build 

make run
```

程序启动后hex_dump 显示约4秒后程序自动切换到跑马灯界面，屏幕中央显示 `22347055 Matrix`，屏幕边缘彩色数字字符沿顺时针方向循环移动。

![image-20260413175056822](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413175056822.png)

> 请在报告中展示对 **GDT 所在内存区域**（地址 `0x8800` 开始，至少32字节）的 hex dump 输出截图，并与 Assignment 2.2 的手工解析结果进行对比验证。

```
实际 hex dump 输出（以数据段描述符 0x8808 为例）：
FF FF 00 00 00 93 CF 00

手工解析（Assignment 2.2，写入时）：
FF FF 00 00 00 92 CF 00
```

除 Accessed 位外，hex dump 结果与手工解析完全一致，GDT 构建正确。Accessed 位由 CPU 硬件在执行 `mov ds/ss/gs, eax` 时自动置位，是 x86 保护模式的正常硬件行为。



![image-20260413175123336](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413175123336.png)



## Assignment4 保护模式下的简易内存浏览器

在 32 位保护模式下，直接操作 8042 键盘控制器的端口来获取按键输入。**状态端口 0x64：** 读取该端口的值，若第 0 位（bit 0）为 1，表示输出缓冲区有数据可读。**数据端口 0x60：** 当缓冲区有数据时，从该端口读取 8 位键盘扫描码（Scan Code）。**Make/Break 码：** 键盘按键按下时产生 Make Code，松开时产生 Break Code（通常为 Make Code 加上 `0x80`，即最高位置 1）。本实验只需响应 Make Code。

主循环机制如下：

程序进入保护模式并完成段寄存器初始化后，进入一个无限循环。

```assembly
browser_loop:
    call clear_screen           ; 1. 清理屏幕残留
    ; ... 打印标题栏和当前地址 ...
    ; ... 打印操作提示 ...
    mov  esi, [cur_addr]
    mov  ecx, 256
    mov  edx, 2
    call hex_dump               ; 2. 将当前地址开始的256字节以 hex dump 格式输出
    call wait_keypress          ; 3. 阻塞等待有效按键
    jmp  browser_loop           ; 4. 循环重绘
```

键盘轮训机制：

```assembly
.poll:
    in   al, 0x64               ; 读状态端口
    test al, 0x01
    jz   .poll                  ; 无数据则循环轮询

    in   al, 0x60               ; 读数据端口(扫描码)
    test al, 0x80
    jnz  .poll                  ; 忽略 Break Code (bit7=1)
```

十六进制内存转储与Assignment3.2相同。



实验结果如下：

![image-20260413214245767](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413214245767.png)

向下翻页：

![image-20260413214335987](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413214335987.png)

向上翻页：

![image-20260413214411992](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413214411992.png)

按q退出：

![image-20260413214446138](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260413214446138.png)

## 思考题

1.

计算机插电后从启动从ROM中读取BIOS，之后BIOS读取设备第一个扇区的512字节的MBR，其中存放的就是Bootloader。其作用是：将 CPU 从 16 位实模式切换到 32 位保护模式，建立全局描述符表（GDT），初始化基础的段寄存器和栈。 由于自身容量受限，它通常需要通过读取硬盘，将真正的操作系统内核（Kernel）或二级引导程序（Loader）从磁盘加载到内存中。

2.

处理器的 4 种主要 I/O 交互方式：

1. **程序直接控制方式（轮询/Programmed I/O，PIO）：** CPU 不断地循环读取状态端口，直到设备准备就绪才进行数据读写。非常消耗 CPU 资源。
2. **中断驱动方式（Interrupt-driven I/O）：** CPU 分发任务后就去干别的，外设准备好数据后向 CPU 发送中断信号，CPU 响应中断再来处理数据。
3. **直接内存访问（DMA，Direct Memory Access）：** 外设和内存之间直接进行数据交换，由 DMA 控制器接管总线，不需要 CPU 介入数据搬运过程。
4. **通道控制方式（Channel I/O）：** 比 DMA 更高级，使用专门的 I/O 处理机（通道）来执行 I/O 指令，CPU 只需发出宏观的 I/O 指令即可（多见于大型机）。

使用 `in` 和 `out` 指令配合 LBA 端口（如 `0x1F0`~`0x1F7`）读取硬盘，**属于“程序直接控制方式（PIO）”**。

3.

out指令操作的立即数只能是8位的，而0x1F3是16位。

4.

**段描述符 (Segment Descriptor)：** 内存里的一个 8 字节（64位）的数据结构。它详细记录了一个内存段的“档案”，包括：段的起始物理地址（基址）、段的长度（界限）、以及这个段的属性（是代码还是数据？能否可写？特权级是 0 还是 3？）。

**GDT (全局描述符表, Global Descriptor Table)：** 顾名思义，它就是一个数组。里面存放着许许多多个“段描述符”。

**GDTR (GDT 寄存器)：** CPU 内部的一个专用寄存器。它存放着 GDT 这个数组在内存中的**物理起始地址**和**表的大小**。CPU 靠它才能找到 GDT 在哪。

**段选择子 (Segment Selector)：** 一个 16 位的数值，通常被加载到段寄存器中（如 CS, DS, ES）。你可以把它看作是 GDT 这个数组的**索引（Index）**。

当程序想要访问内存时，它把**段选择子**放入段寄存器。CPU 通过 **GDTR** 找到 **GDT** 的位置，然后用**段选择子**作为索引，在 GDT 中查找到对应的**段描述符**。最后，CPU 从段描述符中提取出该段的基地址，加上程序给出的偏移量，从而完成内存寻址。

5.

**线性地址 (Linear Address)：** 也叫虚拟地址，是经过“分段机制”转换后得到的地址（即 `段基址 + 偏移地址`）。在平坦模型（Flat Model，段基址为 0）下，偏移地址就是线性地址。它是一个逻辑概念上的连续地址空间。

**物理地址 (Physical Address)：** CPU 地址总线上真实发出的电信号地址，直接对应物理内存（RAM 条）上的真实硬件存储单元。

如果操作系统**没有开启分页机制**（Paging），那么 线性地址 = 物理地址。 如果操作系统**开启了分页机制**，线性地址还需要经过 CPU 的内存管理单元（MMU）进行“页表映射”转换，才能得到最终的物理地址。现代操作系统均通过开启分页来实现虚拟内存空间。

6.

`equ` 是汇编语言中的**伪指令**（Directive），意为 "Equate"（等同于）。 它的作用类似于 C 语言中的 `#define` 宏定义。它只是告诉汇编编译器：“在接下来的代码中，凡是看到这个符号，请直接替换为对应的数值”。 **关键点：** `equ` 不会分配任何内存空间，也不会生成任何机器码指令，它纯粹是给编译器看的，用来提高代码的可读性和可维护性。

7.

在保护模式的平坦模型下，除了视频段，代码段、数据段和栈段的**基址通常都是 0，界限都是 4GB**。它们的区别主要在于**属性（Access Byte / 描述符类型）**：

- **数据段描述符：** 类型属性标记为**可读、可写**。处理器不允许把数据段加载到 `CS`（代码段寄存器）中执行。
- **栈段描述符：** 本质上它也是一个可读可写的数据段。但在严谨的操作系统中，栈段的类型属性可以被特别标记为**向下扩展**（Expand-Down），意味着数据入栈时地址递减。不过在现代平坦模型下，通常直接复用数据段描述符作为栈段。
- **视频段描述符：** 基址不再是 0，而是硬编码指向显存映射区的起始物理地址 **`0xB8000`**。界限通常很小（比如 32KB，刚好覆盖显存大小）。这样一来，将视频段选择子装入 `GS` 寄存器后，往 `[gs:0]` 写数据就等同于直接往屏幕左上角打印字符。

8.

在 32 位 x86 架构下（以最常见的 `cdecl` 调用约定为例），一次 C 语言的函数调用在汇编底层会经历以下剧本：

1. **参数传递（Caller 负责）：** 调用者（Caller）将参数从右向左依次压入栈中（`push` 指令）。
2. **函数跳转（Caller 负责）：** 调用者执行 `call` 指令。`call` 会做两件事：把下一条指令的地址（返回地址）压入栈中，然后将 EIP 跳转到被调用函数（Callee）的入口。
3. **保护现场与开辟局部空间（Callee 的 Prologue / 序言）：**
   - 被调用函数一进来，先执行 `push ebp` 保存旧的栈底指针。
   - 执行 `mov ebp, esp`，将当前的栈顶作为自己这个函数的新栈底（建立新的栈帧）。
   - 执行 `sub esp, N`，将栈顶向下移动 N 个字节，为自己的局部变量腾出空间。
4. **执行逻辑：** 函数执行其核心代码，局部变量通过 `[ebp - 偏移量]` 访问，传进来的参数通过 `[ebp + 偏移量]` 访问。函数的返回值通常放在 `eax` 寄存器中。
5. **恢复现场（Callee 的 Epilogue / 结尾）：**
   - 执行 `mov esp, ebp`（或 `leave` 指令），销毁局部变量空间。
   - 执行 `pop ebp`，恢复调用者原来的栈底指针。
6. **函数返回：** 被调用函数执行 `ret` 指令，CPU 会从栈顶弹出一个值给 EIP，这就回到了刚才 `call` 之后的位置。
7. **清理参数栈（Caller 负责）：** 调用者执行类似 `add esp, 8` 的指令，把第一步压入栈中的参数空间回收掉。

11.

参考前文的Assignment2.1

## 复现 Example2 时遇到的一些问题

### Makefile

```makefile
symbol:
	@nasm -g -f elf32 mbr.asm -o mbr.o
	@${LD} -o mbr.symbol -melf_i386 -N mbr.o -Ttext 0x7c00
	@sed '/^org/d' bootloader.asm > bootloader_noorg.asm
	@nasm -g -f elf32 bootloader_noorg.asm -o bootloader.o
	@${LD} -o bootloader.symbol -melf_i386 -N bootloader.o -Ttext 0x7e00
	@rm -f bootloader_noorg.asm 
```

这里最后一行  `@rm -f bootloader_noorg.asm ` 会删掉bootloader.asm 导致参照appendix文档进行debug时，在`layout src`这一步会导致 `[ No Source Available ]`。
