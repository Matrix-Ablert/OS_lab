# Assignment 1 实验报告：磁盘驱动与 FAT16 基础

## 1. 实验运行方式

本实验复现 `Assignment1/1.1` 中的 FAT16 文件系统示例。为了避免 QEMU 图形窗口输出过长导致截图无法保存完整内容，已增加 `make output` 目标，将 QEMU 串口输出保存到 `Assignment1/output.txt`。

运行命令如下：

```bash
cd Assignment1/1.1/build
make clean
make image
make build
make output
```

其中：

- `make image` 创建 10MB 磁盘镜像 `run/hd.img`。
- `make build` 编译 MBR、Bootloader 和 Kernel，并写入磁盘镜像。
- `make output` 以无图形模式运行 QEMU，并把内核演示输出保存为 `Assignment1/output.txt`。

运行结果应包含 `FAT16 File System Demo`、文件创建、写入、读取、删除、覆盖写以及 `Demo Complete!` 等内容。完整输出见 `output.txt`。

## 2. ATA PIO 磁盘读写流程分析

### 2.1 `waitReady()` 等待磁盘就绪

`DiskDriver::waitReady()` 通过轮询 ATA 状态寄存器 `0x1F7` 判断磁盘是否忙碌：

```cpp
void DiskDriver::waitReady()
{
    uint8 status;
    do
    {
        asm_in_port(ATA_STATUS, &status);
    } while (status & ATA_STATUS_BSY);
}
```

`ATA_STATUS_BSY` 的值是 `0x80`，对应状态寄存器的 BSY 位。当该位为 1 时，磁盘仍在处理上一条命令，不能继续设置 LBA 或发送读写命令。循环直到 BSY 清零，表示磁盘空闲。

### 2.2 `selectSector()` 设置 LBA 地址和扇区数

`selectSector(uint32 lba, uint8 count)` 负责选择要访问的扇区：

```cpp
asm_out_port(ATA_SECT_CNT, count);
asm_out_port(ATA_LBA_LOW, (uint8)(lba & 0xFF));
asm_out_port(ATA_LBA_MID, (uint8)((lba >> 8) & 0xFF));
asm_out_port(ATA_LBA_HIGH, (uint8)((lba >> 16) & 0xFF));
asm_out_port(ATA_DRIVE_HEAD, (uint8)(0xE0 | ((lba >> 24) & 0x0F)));
```

流程为：

1. 先调用 `waitReady()` 等待磁盘空闲。
2. 向 `0x1F2` 写入扇区数量 `count`。
3. 向 `0x1F3`、`0x1F4`、`0x1F5` 写入 LBA 地址的低、中、高 8 位。
4. 向 `0x1F6` 写入 `0xE0 | LBA[27:24]`，选择主盘并启用 LBA 模式。

### 2.3 `readSectors()` 读取扇区

`readSectors()` 对每个扇区重复执行一次 ATA PIO 读取：

1. 调用 `selectSector(lba + s, 1)` 选择当前扇区。
2. 向命令寄存器 `0x1F7` 写入 READ 命令 `0x20`。
3. 轮询 `0x1F7`，直到 DRQ 位 `0x08` 置 1，表示数据已经准备好。
4. 从数据寄存器 `0x1F0` 连续读取 256 个 16-bit word。

一个扇区大小是 512 字节，`512 / 2 = 256`，所以每个扇区需要执行 256 次 `asm_in_port_word(ATA_DATA, ...)`。

### 2.4 `writeSectors()` 写入扇区

`writeSectors()` 与读取流程类似，区别在于命令和数据方向：

1. 调用 `selectSector(lba + s, 1)` 选择当前扇区。
2. 向 `0x1F7` 写入 WRITE 命令 `0x30`。
3. 等待 DRQ 置 1。
4. 将缓冲区中的 256 个 16-bit word 依次通过 `asm_out_port_word(0x1F0, ...)` 写入磁盘数据寄存器。

读流程使用 `in ax, dx` 从磁盘到内存；写流程使用 `out dx, ax` 从内存到磁盘。两者都需要先选择扇区，再发送命令，再等待 DRQ。

### 2.5 为什么 `0x1F0` 使用 16 位 I/O

ATA 数据寄存器 `0x1F0` 是 16 位数据端口，PIO 数据传输的基本单位是 16-bit word，而不是 8-bit byte。若使用 `in al, dx` 或 `out dx, al`，每次只能传输 1 字节，不符合 ATA 数据端口的传输宽度，也会破坏扇区数据读取和写入的节奏。

因此本实验新增：

```asm
in ax, dx
out dx, ax
```

每次传输 2 字节，正好读取或写入一个 ATA word。一个扇区需要 256 次 16 位 I/O。

## 3. FAT16 文件系统布局分析

### 3.1 磁盘布局图

本实验的文件系统从磁盘扇区 `500` 开始，前面的扇区留给 MBR、Bootloader 和 Kernel。

| 区域 | 起始扇区 | 结束扇区 | 大小 | 说明 |
| --- | ---: | ---: | ---: | --- |
| MBR | 0 | 0 | 1 扇区 | 主引导记录 |
| Bootloader | 1 | 5 | 5 扇区 | 引导加载器 |
| Kernel | 6 | 150 | 145 扇区 | 内核镜像写入区域 |
| 保留空间 | 151 | 499 | 349 扇区 | 避免覆盖内核和引导区域 |
| SuperBlock | 500 | 500 | 1 扇区 | 简化 BPB，魔数为 `SF16` |
| FAT 表 | 501 | 532 | 32 扇区 | 每项 2 字节 |
| 根目录区 | 533 | 564 | 32 扇区 | 每项 32 字节 |
| 数据区 | 565 | 后续扇区 | 8190 个可用簇 | 文件内容区，1 簇 = 1 扇区 |

对应代码常量：

```cpp
#define FS_START_SECTOR   500
#define FAT_SECTORS       32
#define ROOT_DIR_SECTORS  32
#define FAT_ENTRIES       8192
#define DATA_START_OFFSET 65
#define SECTOR_SIZE       512
```

其中：

```text
DATA_START_OFFSET = 1 + FAT_SECTORS + ROOT_DIR_SECTORS
                  = 1 + 32 + 32
                  = 65
```

所以数据区起始扇区为：

```text
FS_START_SECTOR + DATA_START_OFFSET = 500 + 65 = 565
```

### 3.2 FAT 表条目数量和可管理簇数

FAT16 表中每个条目占 2 字节。FAT 表共 32 个扇区，每个扇区 512 字节：

```text
每扇区 FAT 条目数 = 512 / 2 = 256
FAT 条目总数 = 32 * 256 = 8192
```

FAT 条目 0 和 1 是保留条目，不对应实际数据簇；有效数据簇从 2 开始。因此最多可以管理：

```text
8192 - 2 = 8190 个数据簇
```

本实验中 1 簇 = 1 扇区 = 512 字节，所以数据区最大可用容量约为：

```text
8190 * 512 = 4193280 字节
```

### 3.3 根目录容量

根目录区占 32 个扇区，每个目录条目大小为 32 字节：

```text
每扇区目录条目数 = 512 / 32 = 16
根目录最大条目数 = 32 * 16 = 512
```

因此根目录最多能存放 512 个文件或目录项。本实验只实现单级根目录，不支持子目录。

### 3.4 FAT 链示例

如果一个文件占用的簇链是：

```text
5 -> 8 -> 12 -> EOF
```

则 FAT 表中应保存：

```text
FAT[5]  = 8
FAT[8]  = 12
FAT[12] = 0xFFF8
```

其中 `0xFFF8` 是本实验定义的 `FAT16_EOF`，表示文件结束。

### 3.5 `clusterToSector()` 计算逻辑

代码如下：

```cpp
uint32 FAT16::clusterToSector(uint16 cluster)
{
    return FS_START_SECTOR + DATA_START_OFFSET + (cluster - 2);
}
```

含义是：

- `FS_START_SECTOR`：文件系统起始扇区，本实验为 500。
- `DATA_START_OFFSET`：数据区相对文件系统起始位置的偏移，本实验为 65。
- `cluster - 2`：FAT 约定簇号 0 和 1 保留，簇 2 才是第一个有效数据簇。

因此：

```text
cluster 2 -> sector 500 + 65 + 0 = 565
cluster 3 -> sector 500 + 65 + 1 = 566
cluster 4 -> sector 500 + 65 + 2 = 567
```

这个公式把 FAT 中的逻辑簇号映射到磁盘上的实际 LBA 扇区号。

## 4. 实验结论

本实验通过 ATA PIO 端口 I/O 实现了基础磁盘读写，再在磁盘扇区之上构建了简化 FAT16 文件系统。磁盘驱动负责按 LBA 读写 512 字节扇区，FAT16 层负责用 FAT 表维护簇链，用根目录项保存文件名、起始簇号和文件大小。通过 `make output` 生成的 `output.txt` 可以完整保存 FAT16 演示流程，解决了 QEMU 图形窗口截图无法覆盖全部输出的问题。
