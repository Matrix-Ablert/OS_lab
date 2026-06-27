# Assignment 2 实验报告：文件操作实现分析

## 1. 实验运行方式

本实验复用 Assignment1 的 FAT16 示例工程，并在 `Assignment2/2.3/src/kernel/shell.cpp` 的 `runDemo()` 中加入 Assignment2 要求的自定义文件操作测试。运行命令如下：

```bash
cd Assignment2/2.3/build
make clean
make image
make build
make output
```

`make output` 会通过 QEMU 串口输出生成完整日志：

```text
Assignment2/output.txt
```

这样可以保存完整运行结果，避免 QEMU 图形界面输出过长导致截图无法覆盖全部内容。

## 2.1 文件创建与写入分析

### 2.1.1 `createFile("hello.txt")` 执行流程

`createFile()` 首先检查文件系统是否已经挂载。如果 `mounted == false`，直接输出错误并返回。

随后调用 `findEntry(name)` 检查同名文件是否已经存在。`findEntry()` 内部会先调用 `toFAT16Name()`，把普通文件名转换为 FAT16 的 8.3 格式。

以 `hello.txt` 为例：

```text
输入文件名: hello.txt
文件名部分: HELLO + 3 个空格
扩展名部分: TXT
FAT16 11 字节格式: "HELLO   TXT"
```

转换规则如下：

1. 先把 11 字节目标缓冲区全部填为空格。
2. 点号前最多复制 8 个字符作为主文件名。
3. 小写字母转换成大写字母。
4. 点号后最多复制 3 个字符作为扩展名。
5. 点号本身不存入目录项。

如果目录中没有同名文件，`createFile()` 调用 `findFreeEntry()` 查找空闲根目录项。空闲条件有两个：

```cpp
entry.filename[0] == 0x00
entry.filename[0] == ATTR_DELETED
```

其中 `0x00` 表示从该项开始后续目录项都未使用，`0xE5` 表示该目录项曾经被删除，可以复用。

找到空闲目录项后，`createFile()` 初始化一个 `DirEntry`：

- `filename[8]`：写入 8 字节主文件名。
- `extension[3]`：写入 3 字节扩展名。
- `attributes = ATTR_ARCHIVE`：普通文件。
- `firstCluster = 0`：空文件暂不占用数据簇。
- `fileSize = 0`：文件大小为 0。

最后调用 `writeDirEntry(idx, &entry)` 将目录项写回根目录区。

### 2.1.2 `writeFile("hello.txt", data, size)` 执行流程

`writeFile()` 是覆盖写模式，主要步骤如下。

第一步，查找目录项：

- 如果文件不存在，则类似 `createFile()` 创建新的目录项。
- 如果文件已存在，则读取旧目录项。

第二步，覆盖写时释放旧簇链：

```cpp
if (entry.firstCluster >= 2)
{
    freeClusterChain(entry.firstCluster);
}
```

`freeClusterChain()` 从起始簇开始沿 FAT 表向后遍历，把每个簇对应的 FAT 条目改成 `FAT16_FREE`，直到遇到 EOF 或无效簇号。这样旧文件内容占用的空间重新变为空闲。

第三步，计算新内容需要的簇数。由于本实验中 1 簇 = 1 扇区 = 512 字节，计算方式为：

```cpp
clustersNeeded = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
```

这相当于向上取整。例如：

- `size = 1`，需要 1 个簇。
- `size = 512`，需要 1 个簇。
- `size = 513`，需要 2 个簇。

第四步，调用 `allocateCluster()` 分配簇。`allocateCluster()` 从簇 2 开始扫描 FAT 表，找到第一个值为 `FAT16_FREE` 的条目，将其标记为 `FAT16_EOF` 并返回簇号。簇 0 和簇 1 是 FAT 保留簇，因此不会分配给文件。

第五步，构建 FAT 链。每分配一个新簇：

- 第一个簇记录为 `firstCluster`。
- 从第二个簇开始，把上一个簇的 FAT 条目改为当前簇号。
- 最后一个簇保持 `FAT16_EOF`。

例如一个文件需要 3 个簇，分配到 5、8、12，则 FAT 链为：

```text
FAT[5]  = 8
FAT[8]  = 12
FAT[12] = 0xFFF8
```

第六步，写入数据。代码将数据分块复制到 512 字节扇区缓冲区，再写入对应簇所在扇区：

```cpp
disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);
```

本项目的 `memcpy(src, dst, length)` 参数顺序是源地址在前、目标地址在后，与标准 C 库 `memcpy(dst, src, length)` 不同。因此代码中的：

```cpp
memcpy((void *)(data + i * SECTOR_SIZE), sectorBuf, copySize);
```

表示把 `data` 中的一段内容复制到 `sectorBuf`，然后写入磁盘。

第七步，更新目录项并同步 FAT：

- `entry.firstCluster = firstCluster`
- `entry.fileSize = size`
- `writeDirEntry(idx, &entry)`
- `syncFAT()`

必须调用 `syncFAT()`，因为 FAT 表先在内存数组 `fatTable` 中修改。如果不写回磁盘，重启或重新挂载后磁盘上的 FAT 表仍是旧状态，文件簇链会丢失或不一致。

## 2.2 文件读取与删除分析

### 2.2.1 `readFile("hello.txt", buffer, maxSize)` 执行流程

`readFile()` 首先检查挂载状态，然后调用 `findEntry(name)` 查找根目录项。找到后通过 `readDirEntry(idx, &entry)` 读取目录项。

目录项中最关键的两个字段是：

- `entry.firstCluster`：文件数据的第一个簇号。
- `entry.fileSize`：文件真实字节数。

函数先计算实际最多读取多少字节：

```cpp
readSize = entry.fileSize;
if (readSize > maxSize)
    readSize = maxSize;
```

因此读取不会超过文件大小，也不会超过调用者提供的缓冲区容量。

随后从 `firstCluster` 开始沿 FAT 链逐簇读取：

1. 调用 `clusterToSector(cluster)` 把簇号转换为扇区号。
2. 调用 `disk->readSectors(..., 1, sectorBuf)` 读取该簇对应扇区。
3. 将本次需要的字节复制到用户缓冲区。
4. 通过 `cluster = fatTable[cluster]` 跳到下一个簇。

读取停止条件有两个：

- 当前簇号进入 EOF 区间，即 `cluster >= FAT16_BAD`。
- 已读取字节数达到 `readSize`，也就是达到文件大小或 `maxSize`。

### 2.2.2 `deleteFile("data.bin")` 执行流程

`deleteFile()` 先通过 `findEntry(name)` 查找目录项。如果找不到，输出 `file not found` 并返回失败。

找到目录项后，如果文件有数据簇，则调用：

```cpp
freeClusterChain(entry.firstCluster);
syncFAT();
```

`freeClusterChain()` 沿 FAT 链释放所有簇，把这些簇重新标记为 `FAT16_FREE`。随后 `syncFAT()` 将修改后的 FAT 表写回磁盘。

接着，删除目录项并不是清空整个目录项，而是只修改首字节：

```cpp
entry.filename[0] = (char)ATTR_DELETED;
writeDirEntry(idx, &entry);
```

`ATTR_DELETED` 的值是 `0xE5`。后续 `listFiles()` 和 `findEntry()` 会跳过首字节为 `0xE5` 的目录项，而 `findFreeEntry()` 可以重新使用该位置。

删除文件后，文件内容并没有真的从磁盘数据区擦除。删除操作只做了两件事：

1. FAT 表中对应簇被标记为空闲。
2. 目录项首字节被标记为删除。

数据区原本的字节仍然留在磁盘上，直到这些簇被新文件重新分配并覆盖。因此 FAT 文件系统中删除文件通常存在数据恢复可能；如果需要安全删除，必须主动覆盖原数据簇。

## 2.3 自定义文件操作测试

本实验修改 `runDemo()`，加入以下测试流程。

### 2.3.1 创建并写入 5 个文件

创建文件：

```text
alpha.txt
beta.txt
gamma.txt
delta.txt
echo.txt
```

每个文件写入不同内容：

```text
alpha: first file content
beta: second file content
gamma: third file content
delta: fourth file content
echo: fifth file content
```

这些内容都小于 512 字节，因此每个文件只需要 1 个簇。初次写入后，`listFiles()` 应显示 5 个文件，并显示对应大小和起始簇。

### 2.3.2 读取验证

程序逐个调用 `readFile()` 读取 5 个文件，并打印读取到的内容。输出内容与写入内容一致，说明：

- 目录项中的文件名和大小正确。
- `firstCluster` 指向正确的数据簇。
- FAT 链遍历和 `disk->readSectors()` 读取流程正确。

### 2.3.3 删除并重建同名文件

随后删除：

```text
beta.txt
delta.txt
```

删除后再次 `listFiles()`，目录中只剩 3 个文件：

```text
alpha.txt
gamma.txt
echo.txt
```

然后重新创建同名文件：

```text
beta.txt
delta.txt
```

并写入新内容：

```text
beta: recreated content
delta: recreated content
```

再次 `listFiles()` 后，目录恢复为 5 个文件。由于 `findFreeEntry()` 会复用 `0xE5` 标记的目录项，`allocateCluster()` 又会从簇 2 开始扫描空闲簇，因此被删除文件释放出的目录项和簇可以被新文件重新使用。

完整运行结果保存在：

```text
Assignment2/output.txt
```

## 3. 实验结论

Assignment2 验证了简化 FAT16 文件系统的核心文件操作。`createFile()` 负责建立目录项，`writeFile()` 负责分配簇链并写入数据，`readFile()` 通过目录项和 FAT 链找回文件内容，`deleteFile()` 释放 FAT 链并用 `0xE5` 标记目录项。自定义测试进一步证明：多个文件可以正确创建、写入、读取和列出；删除文件后，其目录项和簇空间可以被同名文件重新利用。
