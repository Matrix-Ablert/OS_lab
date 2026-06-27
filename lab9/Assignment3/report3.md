# Assignment 3 实验报告：文件系统功能扩展

## 1. 实验组织方式

Assignment3 按小任务拆成三个独立工程：

```text
Assignment3/3.1  文件追加写入 appendFile
Assignment3/3.2  文件簇数统计与碎片率统计
Assignment3/3.3  文件重命名 renameFile
```

每个子任务都复用前面 Assignment 的 FAT16 工程和串口输出机制，但 `make output` 的输出路径改为当前子任务目录：

```text
Assignment3/3.1/output.txt
Assignment3/3.2/output.txt
Assignment3/3.3/output.txt
```

运行方式分别为：

```bash
cd Assignment3/3.1/build
make clean && make image && make build && make output

cd Assignment3/3.2/build
make clean && make image && make build && make output

cd Assignment3/3.3/build
make clean && make image && make build && make output
```

## 2. Assignment 3.1：文件追加写入

### 2.1 接口

在 `FAT16` 类中新增：

```cpp
bool appendFile(const char *name, const char *data, uint32 size);
```

### 2.2 实现思路

如果文件不存在，`appendFile()` 直接调用：

```cpp
writeFile(name, data, size);
```

这样新文件追加写与普通写入行为一致。

如果文件已经存在，则先读取目录项，取得：

- `firstCluster`
- `fileSize`

然后沿 FAT 链找到最后一个簇：

```cpp
while (fatTable[lastCluster] >= 2 && fatTable[lastCluster] < FAT16_BAD)
    lastCluster = fatTable[lastCluster];
```

追加写分两种情况处理：

1. 如果最后一个簇还有剩余空间，先读出该簇对应扇区，把新数据复制到 `fileSize % 512` 的偏移处，再写回该扇区。
2. 如果还有剩余数据，则调用 `allocateCluster()` 分配新簇，将原链尾的 FAT 表项指向新簇，并把新簇作为新的链尾。

写完后更新：

```cpp
entry.fileSize += size;
writeDirEntry(idx, &entry);
syncFAT();
```

必须调用 `syncFAT()`，否则新分配的簇链只存在于内存中的 `fatTable`，不会持久化到磁盘。

### 2.3 测试结果

`runDemo()` 测试：

1. 对不存在的 `append.txt` 调用 `appendFile()`。
2. 向同一文件追加 620 字节数据，触发跨簇写入。
3. 再追加尾部字符串。
4. 读取最终内容，验证拼接后的文件大小和内容。

完整输出见：

```text
Assignment3/3.1/output.txt
```

## 3. Assignment 3.2：磁盘空间统计

### 3.1 接口

新增：

```cpp
int FAT16::getFileClusterCount(const char *name);
float FAT16::getFragmentation();
```

### 3.2 文件簇数统计

`getFileClusterCount()` 先查找目录项，然后从 `firstCluster` 开始沿 FAT 链计数：

```cpp
while (cluster >= 2 && cluster < FAT16_BAD)
{
    count++;
    cluster = fatTable[cluster];
}
```

空文件返回 0；文件不存在返回 -1。

### 3.3 碎片率统计

`getFragmentation()` 遍历根目录中所有有效文件，沿每个文件的 FAT 链统计相邻簇链接。

如果下一簇不是当前簇号加 1，则说明该链路不连续：

```cpp
if (next != current + 1)
    fragmentedLinks++;
```

碎片率计算为：

```text
fragmentedLinks / totalLinks
```

如果没有多簇文件，即 `totalLinks == 0`，返回 0。

### 3.4 测试结果

`runDemo()` 测试：

1. 创建 `first.bin`、`gap.bin`、`tail.bin`，初始簇链连续，碎片率为 0。
2. 删除 `gap.bin`，在中间制造空洞。
3. 创建更大的 `new.bin`，它会先使用空洞中的簇，再跳到后续空闲簇，因此 FAT 链出现不连续。
4. 打印每个文件占用簇数和碎片率百分比。

完整输出见：

```text
Assignment3/3.2/output.txt
```

## 4. Assignment 3.3：文件重命名

### 4.1 接口

新增：

```cpp
bool FAT16::renameFile(const char *oldName, const char *newName);
```

### 4.2 实现思路

`renameFile()` 只修改目录项，不移动数据、不修改 FAT 链。

流程：

1. 检查文件系统已挂载。
2. 调用 `findEntry(oldName)` 确认旧文件存在。
3. 调用 `findEntry(newName)` 确认新文件名不冲突。
4. 用 `toFAT16Name(newName, fat16Name)` 转换为 FAT16 8.3 名称。
5. 覆盖目录项中的 `filename[8]` 和 `extension[3]`。
6. 调用 `writeDirEntry()` 写回目录项。

由于 `firstCluster` 和 `fileSize` 不变，重命名后文件内容仍可通过新的文件名读取。

### 4.3 测试结果

`runDemo()` 测试：

1. 创建 `old.txt` 和 `other.txt`。
2. 将 `old.txt` 重命名为 `renamed.txt`。
3. 列目录确认文件名变化。
4. 用 `renamed.txt` 读取内容，验证数据未变化。
5. 用 `old.txt` 读取，验证旧名字失效。
6. 尝试把 `renamed.txt` 重命名为已存在的 `other.txt`，验证冲突失败。

完整输出见：

```text
Assignment3/3.3/output.txt
```

## 5. 实验结论

Assignment3 在原有 FAT16 基础上扩展了三个典型文件系统功能。`appendFile()` 体现了 FAT 链尾部扩展和部分簇写入；空间统计通过 FAT 链遍历获得文件簇数和碎片率；`renameFile()` 说明 FAT 文件名属于目录项元数据，重命名不需要移动数据区内容。三个小任务均独立运行并生成各自的 `output.txt`，便于分别提交和检查。
