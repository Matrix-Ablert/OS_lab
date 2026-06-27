# <center>Lab9 FAT16 文件系统实验报告</center>

> 实验环境：待补充

**本次实验部分代码、注释和报告内容参考自大模型辅助。**

本实验在已有操作系统内核基础上实现简化 FAT16 文件系统。实验内容包括 ATA PIO 磁盘驱动、FAT16 磁盘布局、文件创建/写入/读取/删除分析，以及追加写、碎片率统计、重命名等功能扩展。由于 QEMU 图形窗口输出较长，实验中额外通过串口输出和 `make output` 生成文本日志，完整运行结果保存到各 Assignment 的 `output.txt` 中。

## Assignment 1 磁盘驱动与 FAT16 基础

### 1.1 复现并运行 FAT16 示例

本部分复现 FAT16 文件系统示例，并通过 `make output` 保存完整 QEMU 输出。

**运行命令：**

```bash
cd Assignment1/1.1/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment1/output.txt
```

**关键输出：**

```text
FAT16 File System Demo
[Step 1] Formatting file system...
Created: hello.txt
Created: data.bin
Created: readme
Error: file 'data.bin' not found
Demo Complete!
```

**结果分析：** 输出中可以看到文件系统格式化、文件创建、读取、删除、覆盖写以及读取已删除文件失败等流程，说明磁盘驱动和 FAT16 基本文件操作可以正常运行。`make output` 使用 QEMU 串口输出保存完整日志，避免只依赖 VGA 截图导致内容不完整。

### 1.2 ATA PIO 磁盘读写流程

**实现思路：** ATA PIO 模式通过 I/O 端口直接控制磁盘。实验中的磁盘驱动主要由 `waitReady()`、`selectSector()`、`readSectors()` 和 `writeSectors()` 组成。

`waitReady()` 轮询状态寄存器 `0x1F7`：

```cpp
do
{
    asm_in_port(ATA_STATUS, &status);
} while (status & ATA_STATUS_BSY);
```

其中 `ATA_STATUS_BSY = 0x80`。当 BSY 位为 1 时，表示磁盘正在忙碌，不能继续发送命令；循环结束说明磁盘已经空闲。

`selectSector()` 设置 LBA 地址和扇区数：

```cpp
asm_out_port(ATA_SECT_CNT, count);
asm_out_port(ATA_LBA_LOW, (uint8)(lba & 0xFF));
asm_out_port(ATA_LBA_MID, (uint8)((lba >> 8) & 0xFF));
asm_out_port(ATA_LBA_HIGH, (uint8)((lba >> 16) & 0xFF));
asm_out_port(ATA_DRIVE_HEAD, (uint8)(0xE0 | ((lba >> 24) & 0x0F)));
```

各端口含义如下：

| 端口 | 作用 |
| --- | --- |
| `0x1F2` | 扇区数量 |
| `0x1F3` | LBA 低 8 位 |
| `0x1F4` | LBA 中 8 位 |
| `0x1F5` | LBA 高 8 位 |
| `0x1F6` | 主盘选择、LBA 模式和 LBA 高 4 位 |
| `0x1F7` | 状态寄存器或命令寄存器 |

`readSectors()` 的流程是：

1. 调用 `selectSector(lba + s, 1)` 选择扇区。
2. 向 `0x1F7` 写入 READ 命令 `0x20`。
3. 等待状态寄存器 DRQ 位 `0x08` 置 1。
4. 从数据端口 `0x1F0` 读取 256 个 16-bit word。

`writeSectors()` 与读取类似，区别是向 `0x1F7` 写入 WRITE 命令 `0x30`，并使用 `asm_out_port_word()` 把内存数据写入 `0x1F0`。

**为什么 `0x1F0` 使用 16 位 I/O：** ATA 数据寄存器 `0x1F0` 的传输单位是 16-bit word。一个扇区为 512 字节，因此每个扇区需要传输 `512 / 2 = 256` 个 word。使用 `in ax, dx` 和 `out dx, ax` 能与 ATA PIO 的数据宽度匹配；如果改用 8 位 I/O，只能传输单字节，会破坏数据传输节奏和扇区内容。

### 1.3 FAT16 文件系统布局

本实验的文件系统从磁盘扇区 `500` 开始，前面区域保留给 MBR、Bootloader 和 Kernel。

| 区域 | 起始扇区 | 结束扇区 | 大小 | 说明 |
| --- | ---: | ---: | ---: | --- |
| MBR | 0 | 0 | 1 扇区 | 主引导记录 |
| Bootloader | 1 | 5 | 5 扇区 | 引导加载器 |
| Kernel | 6 | 150 | 145 扇区 | 内核镜像写入区域 |
| 保留空间 | 151 | 499 | 349 扇区 | 避免覆盖引导和内核区域 |
| SuperBlock | 500 | 500 | 1 扇区 | 魔数为 `SF16` |
| FAT 表 | 501 | 532 | 32 扇区 | 每项 2 字节 |
| 根目录区 | 533 | 564 | 32 扇区 | 每项 32 字节 |
| 数据区 | 565 | 后续扇区 | 8190 个可用簇 | 文件内容区 |

对应常量为：

```cpp
#define FS_START_SECTOR   500
#define FAT_SECTORS       32
#define ROOT_DIR_SECTORS  32
#define FAT_ENTRIES       8192
#define DATA_START_OFFSET 65
#define SECTOR_SIZE       512
```

FAT 表每项 2 字节，32 个扇区共可保存：

```text
32 * 512 / 2 = 8192 个 FAT 条目
```

其中簇 0 和簇 1 是保留簇，因此最多管理：

```text
8192 - 2 = 8190 个数据簇
```

根目录区占 32 个扇区，每个目录项 32 字节：

```text
32 * 512 / 32 = 512 个目录项
```

如果文件簇链为 `5 -> 8 -> 12 -> EOF`，则 FAT 表内容为：

```text
FAT[5]  = 8
FAT[8]  = 12
FAT[12] = 0xFFF8
```

`clusterToSector()` 的计算公式为：

```cpp
FS_START_SECTOR + DATA_START_OFFSET + (cluster - 2)
```

`FS_START_SECTOR + DATA_START_OFFSET = 565` 是数据区起始扇区。FAT 约定簇号 0 和 1 保留，因此簇 2 才对应数据区第一个扇区，即扇区 565。

## Assignment 2 文件操作实现分析

### 2.1 文件创建与写入

本部分分析 `createFile()` 和 `writeFile()` 的执行流程。

**`createFile("hello.txt")` 流程：**

1. 检查文件系统是否挂载。
2. 调用 `findEntry(name)` 检查同名文件是否存在。
3. 调用 `toFAT16Name()` 将普通文件名转换为 FAT16 8.3 格式。
4. 调用 `findFreeEntry()` 查找空闲目录项。
5. 初始化目录项并写回根目录区。

以 `hello.txt` 为例，8.3 名称转换结果为：

```text
输入文件名: hello.txt
主文件名: HELLO + 3 个空格
扩展名: TXT
目录项中 11 字节格式: "HELLO   TXT"
```

空闲目录项有两类：首字节为 `0x00` 表示从该项开始尚未使用，首字节为 `0xE5` 表示该项曾被删除，可以复用。

新目录项的关键字段为：

| 字段 | 含义 |
| --- | --- |
| `filename[8]` | 8 字节主文件名 |
| `extension[3]` | 3 字节扩展名 |
| `attributes` | 普通文件使用 `ATTR_ARCHIVE` |
| `firstCluster` | 空文件初始为 0 |
| `fileSize` | 初始为 0 |

**`writeFile()` 流程：** `writeFile()` 是覆盖写模式。如果文件不存在，则先创建目录项；如果文件已存在，则先释放旧簇链。

```cpp
if (entry.firstCluster >= 2)
{
    freeClusterChain(entry.firstCluster);
}
```

随后根据写入大小计算需要的簇数：

```cpp
clustersNeeded = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
```

由于本实验中 1 簇 = 1 扇区 = 512 字节，因此这是对 `size / 512` 向上取整。`allocateCluster()` 从簇号 2 开始扫描 FAT 表，找到空闲项后标记为 `FAT16_EOF`。多个簇会通过 FAT 表链接成链，例如：

```text
FAT[A] = B
FAT[B] = C
FAT[C] = 0xFFF8
```

数据写入时，代码将待写数据复制到 512 字节扇区缓冲区，再调用：

```cpp
disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);
```

最后更新目录项中的 `firstCluster` 和 `fileSize`，并调用 `syncFAT()` 将内存中的 FAT 表写回磁盘。若不调用 `syncFAT()`，磁盘上的 FAT 链不会更新，重新挂载后文件数据可能无法被正确找到。

### 2.2 文件读取与删除

**`readFile()` 流程：**

1. 通过 `findEntry()` 查找目录项。
2. 从目录项取得 `firstCluster` 和 `fileSize`。
3. 读取长度取 `fileSize` 和 `maxSize` 的较小值。
4. 从 `firstCluster` 开始，沿 FAT 链逐簇读取。
5. 当遇到 EOF 标记或读取字节数达到目标长度时停止。

核心逻辑是：

```text
读取当前 cluster 对应扇区
复制到用户 buffer
cluster = fatTable[cluster]
```

**`deleteFile()` 流程：**

删除文件时先调用 `freeClusterChain()` 释放文件占用的 FAT 链，再把目录项首字节改成 `0xE5`：

```cpp
entry.filename[0] = (char)ATTR_DELETED;
writeDirEntry(idx, &entry);
```

删除不会真正擦除数据区内容，只是让 FAT 表中的簇重新变为空闲，并让目录项不再被普通查找逻辑识别。因此，在这些簇被新文件覆盖之前，原始数据仍可能留在磁盘镜像中。

### 2.3 自定义文件操作测试

本部分修改 `runDemo()`，创建 5 个文件，分别写入不同内容，再验证读取、删除和同名重建。

**运行命令：**

```bash
cd Assignment2/2.3/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment2/output.txt
```

**关键输出：**

```text
Assignment2 FAT16 File Operation Test
Created: alpha.txt
Created: beta.txt
Created: gamma.txt
Created: delta.txt
Created: echo.txt
alpha.txt  25  2
beta.txt  25  3
delta.txt  26  5
echo.txt  24  6
Deleted: beta.txt
Deleted: delta.txt
Recreated beta.txt with 23 bytes
Recreated delta.txt with 24 bytes
beta.txt  23  3
delta.txt  24  5
Demo Complete!
```

**结果分析：** 删除 `beta.txt` 和 `delta.txt` 后重新创建同名文件，列表中可以看到 `beta.txt` 仍使用簇 3，`delta.txt` 仍使用簇 5，说明被删除目录项和释放后的簇都能被重新利用。读取输出与写入字符串一致，说明文件内容读写正确。

## Assignment 3 文件系统功能扩展

### 3.1 文件追加写入

本部分新增：

```cpp
bool FAT16::appendFile(const char *name, const char *data, uint32 size);
```

**实现思路：** 如果文件不存在，`appendFile()` 直接调用 `writeFile()`，行为等价于创建并写入。如果文件存在，则读取目录项，沿 FAT 链找到最后一个簇，根据 `fileSize % 512` 判断链尾簇是否还有剩余空间。

追加写分为两部分：

1. 若最后一个簇未满，先读出该扇区，把新数据复制到尾部空闲位置，再写回。
2. 若仍有剩余数据，则继续分配新簇，把旧链尾 FAT 项指向新簇，新簇标记为 EOF。

写入完成后更新 `fileSize`、目录项和 FAT 表。

**运行命令：**

```bash
cd Assignment3/3.1/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment3/3.1/output.txt
```

**关键输出：**

```text
Assignment3.1 appendFile Test
append missing file: append.txt += 5 bytes
append existing file: append.txt += 620 bytes
append existing file: append.txt += 5 bytes
append.txt  630  2
append.txt final size: 630 bytes
Demo Complete!
```

**结果分析：** 第一次追加在文件不存在时完成创建写入，第二次追加 620 字节触发跨簇扩展，第三次追加尾部内容。最终文件大小为 `5 + 620 + 5 = 630` 字节，说明尾簇补写和新簇链接都生效。

### 3.2 磁盘空间统计

本部分新增：

```cpp
int FAT16::getFileClusterCount(const char *name);
float FAT16::getFragmentation();
```

`getFileClusterCount()` 查找目录项后，从 `firstCluster` 开始沿 FAT 链计数。空文件返回 0，文件不存在返回 -1。

`getFragmentation()` 遍历根目录有效文件的 FAT 链，只统计文件内部相邻簇链接。如果 `next != current + 1`，说明该链路不连续，记为一次碎片。计算公式为：

```text
碎片率 = 不连续链接数 / 总链接数
```

如果没有多簇链接，则碎片率返回 0。

**运行命令：**

```bash
cd Assignment3/3.2/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment3/3.2/output.txt
```

**关键输出：**

```text
Cluster counts before fragmentation:
fragmentation     : 0%
Deleted: gap.bin
Cluster counts and fragmentation after new.bin:
fragmentation     : 16%
Demo Complete!
```

**结果分析：** 初始创建的多簇文件按顺序分配，碎片率为 0。删除 `gap.bin` 后中间出现空洞，再创建更大的 `new.bin` 时会先使用空洞中的簇，再跳到后续空闲簇，因此文件内部 FAT 链出现不连续，碎片率上升到 16%。

### 3.3 文件重命名

本部分新增：

```cpp
bool FAT16::renameFile(const char *oldName, const char *newName);
```

**实现思路：** 重命名只修改目录项中的 8.3 文件名，不移动数据区，也不修改 FAT 链。

执行流程为：

1. 检查旧文件是否存在。
2. 检查新文件名是否冲突。
3. 调用 `toFAT16Name()` 将新名字转换为 8.3 格式。
4. 覆盖目录项的 `filename[8]` 和 `extension[3]`。
5. 写回目录项。

**运行命令：**

```bash
cd Assignment3/3.3/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment3/3.3/output.txt
```

**关键输出：**

```text
Assignment3.3 renameFile Test
Created old.txt and other.txt
Rename success: old.txt -> renamed.txt
renamed.txt  36  2
renamed.txt (36 bytes): rename keeps file data and FAT chain
Error: file 'old.txt' not found
Conflict rename correctly failed
Demo Complete!
```

**结果分析：** 重命名后 `renamed.txt` 的起始簇仍为 2，读取内容保持不变，说明数据区和 FAT 链没有移动。旧文件名读取失败说明目录项名称已更新；重命名到已存在的 `other.txt` 失败，说明冲突检查生效。

## Assignment 4 选做扩展

### 4.1 交互式 Shell

4.1 增加键盘 IRQ1 和命令解析支持。键盘中断处理流程包括：汇编入口保存现场，发送 EOI，调用 C 处理函数；C 处理函数读取 8042 键盘数据端口 `0x60`，用扫描码表转换 ASCII，并维护输入缓冲区。

Shell 支持如下命令：

```text
ls
cat <filename>
touch <filename>
write <filename> <content>
rm <filename>
info
format
help
```

由于 `make output` 是非交互模式，测试中使用脚本化命令调用同一套命令解析器；真实交互可通过 `make run` 使用。

**运行命令：**

```bash
cd Assignment4/4.1/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment4/4.1/output.txt
```

**关键输出：**

```text
Assignment4.1 Interactive Shell Test
summer> help
Commands: ls, cat <file>, touch <file>, write <file> <content>, rm <file>, info, format, help
summer> write note.txt hello-shell
summer> cat note.txt
hello-shell
summer> rm note.txt
summer> ls
Total: 0 file(s)
Keyboard IRQ1 and line buffer are installed for make run.
Demo Complete!
```

**结果分析：** 脚本化命令依次完成帮助输出、文件创建、写入、列表、读取、删除和格式化，说明命令解析器能够调用 FAT16 文件操作。最后输出键盘 IRQ1 已安装，说明 `make run` 时具备真实键盘输入的基础。

### 4.2 子目录支持

4.2 使用 `ATTR_DIRECTORY` 标记目录项。目录本身占用一个数据簇，该簇中保存目录项，并初始化 `.` 和 `..`。新增接口包括：

```cpp
bool mkdir(const char *path);
bool rmdir(const char *path);
void listFiles(const char *path);
```

同时让 `createFile()`、`writeFile()`、`readFile()` 和 `deleteFile()` 支持一级相对路径，例如：

```text
docs/readme.txt
```

`rmdir()` 只允许删除空目录。如果目录中存在除 `.`、`..` 和已删除项以外的有效目录项，则拒绝删除。

**运行命令：**

```bash
cd Assignment4/4.2/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment4/4.2/output.txt
```

**关键输出：**

```text
mkdir docs
wrote docs/readme.txt (25 bytes)
readme.txt  25  3
docs/readme.txt (25 bytes): hello from a subdirectory
deleted docs/readme.txt
Demo Complete!
```

**结果分析：** 输出中先创建 `docs` 目录，再在其中写入并读取 `readme.txt`，说明一级路径解析和子目录目录项访问有效。删除目录前必须先删除子文件，说明空目录检查符合设计。

### 4.3 大文件与批量 I/O 优化

4.3 创建 1500 字节文件验证跨簇读写。由于 1 簇为 512 字节，1500 字节文件需要 3 个簇。为了观察优化效果，在 `DiskDriver` 中加入 I/O 计数器：

```cpp
resetCounters()
getCommandCount()
getWordIOCount()
```

普通读写逐簇发起 I/O；优化读写在 FAT 链中遇到连续簇时，将连续簇合并为一次多扇区读写，以减少 ATA 命令次数。

**运行命令：**

```bash
cd Assignment4/4.3/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment4/4.3/output.txt
```

**关键输出：**

```text
normal write commands: 39
normal write word IO : 9984
normal read bytes    : 1500
normal read commands : 5
optimized write commands: 1
optimized write word IO : 768
optimized read bytes    : 1500
optimized read commands : 4
optimized read word IO  : 1536
normal.bin  1500  2
opt.bin  1500  5
Demo Complete!
```

**结果分析：** 普通写入包含元数据和数据写入，命令次数较多；优化写入对连续数据簇进行合并，输出中命令次数降为 1。两种方式都能读取 1500 字节并列出文件，说明跨簇读写正确。

### 4.4 文件系统一致性检查

4.4 新增：

```cpp
void fsck();
void injectFsckTestInconsistency();
```

`fsck()` 扫描根目录文件和 FAT 表，检查三类问题：

1. 文件大小需要的簇数是否能被 FAT 链满足。
2. FAT 中是否存在已使用但没有目录项引用的丢失簇。
3. 是否存在多个文件引用同一簇的交叉链接。

测试中先对干净文件系统运行 `fsck()`，再注入链过早中断、丢失簇、交叉链接三类错误，最后再次运行 `fsck()`。

**运行命令：**

```bash
cd Assignment4/4.4/build
make clean
make image
make build
make output
```

![运行截图](待补充截图路径)

完整输出保存于：

```text
Assignment4/4.4/output.txt
```

**关键输出：**

```text
Running fsck on clean state:
Chain errors : 0
Lost clusters: 0
Cross links  : 0

Running fsck after injected errors:
Chain error: broken.bin stops early at cluster 3
Chain error: broken.bin needs 2 clusters, found 1
Lost cluster: 4 marked used but unreferenced
Cross link: cluster 5 referenced 2 times
Lost cluster: 6 marked used but unreferenced
Lost cluster: 7 marked used but unreferenced
Chain errors : 2
Lost clusters: 3
Cross links  : 1
Demo Complete!
```

**结果分析：** 干净状态下三类错误数量均为 0。注入错误后，`fsck()` 能分别报告链过早结束、丢失簇和交叉链接，说明一致性检查能够发现 FAT 表与目录项引用关系之间的不一致。

## 实验总结

本实验完成了从磁盘驱动到文件系统功能扩展的完整链路。Assignment1 通过 ATA PIO 模式实现磁盘扇区读写，并分析了 FAT16 的超级块、FAT 表、根目录区和数据区布局。Assignment2 分析了文件创建、覆盖写、读取和删除的内部流程，验证了目录项和簇释放后的复用。Assignment3 在基础 FAT16 上扩展了追加写、簇数统计、碎片率统计和重命名功能。Assignment4 进一步实现了交互式 Shell、子目录、大文件批量 I/O 和文件系统一致性检查。

实验中新增的 `make output` 将 QEMU 输出保存为文本日志，解决了图形窗口输出过长、截图无法覆盖全部内容的问题。报告中的截图位置保留为占位，完整运行证据以各目录下的 `output.txt` 为准。
