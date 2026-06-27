# Assignment 4 实验报告：文件系统扩展选做

## 1. 实验组织

Assignment4 按小任务拆成四个独立工程：

```text
Assignment4/4.1  交互式 Shell
Assignment4/4.2  子目录支持
Assignment4/4.3  大文件与批量 I/O 优化
Assignment4/4.4  文件系统一致性检查
```

每个工程的 `make output` 都会把 QEMU 串口日志保存到本小任务目录：

```text
Assignment4/4.1/output.txt
Assignment4/4.2/output.txt
Assignment4/4.3/output.txt
Assignment4/4.4/output.txt
```

## 2. Assignment 4.1：交互式 Shell

4.1 增加了键盘 IRQ1 的基础支持：

- `asm_keyboard_interrupt_handler` 保存现场、发送 EOI，并调用 C 语言处理函数。
- `InterruptManager::enableKeyboardInterrupt()` 解除 8259A 主片 IRQ1 屏蔽。
- `InterruptManager::setKeyboardInterrupt()` 将 IRQ1 映射到键盘中断处理入口。
- `c_keyboard_interrupt_handler()` 从 `0x60` 读取扫描码，通过扫描码表转换为 ASCII，并维护命令行输入缓冲区。

Shell 侧实现命令解析能力，支持：

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

由于 `make output` 是无交互运行，演示中使用脚本化命令调用同一套解析逻辑，完整输出见：

```text
Assignment4/4.1/output.txt
```

## 3. Assignment 4.2：子目录支持

4.2 扩展目录项语义，使用 `ATTR_DIRECTORY` 标记目录。目录自身占用一个数据簇，该簇中保存目录项，并初始化：

```text
.
..
```

新增接口：

```cpp
bool mkdir(const char *path);
bool rmdir(const char *path);
void listFiles(const char *path);
```

同时让 `createFile()`、`writeFile()`、`readFile()`、`deleteFile()` 支持形如：

```text
docs/readme.txt
```

的一级相对路径。`rmdir()` 只允许删除空目录；如果目录中存在普通文件，会输出目录非空错误。

演示流程：

1. `mkdir docs`
2. 写入 `docs/readme.txt`
3. `ls` 根目录和 `ls docs`
4. 读取子目录文件
5. 尝试删除非空目录失败
6. 删除文件后成功删除空目录

完整输出见：

```text
Assignment4/4.2/output.txt
```

## 4. Assignment 4.3：大文件与批量 I/O 优化

4.3 在 `DiskDriver` 中加入 I/O 计数器：

```cpp
resetCounters()
getCommandCount()
getWordIOCount()
```

普通读写中，每个扇区产生一次 ATA READ/WRITE 命令；优化读写中，连续簇会被合并成一次批量读写命令，从而降低命令次数。

新增 FAT16 优化接口：

```cpp
bool writeFileOptimized(const char *name, const char *data, uint32 size);
int readFileOptimized(const char *name, char *buffer, uint32 maxSize);
```

演示创建 1500 字节文件，超过 1 个簇，验证跨簇读写，并输出普通读写与优化读写的命令计数和 16-bit 数据端口传输次数。

完整输出见：

```text
Assignment4/4.3/output.txt
```

## 5. Assignment 4.4：文件系统一致性检查

4.4 新增：

```cpp
void fsck();
void injectFsckTestInconsistency();
```

`fsck()` 会扫描根目录文件和 FAT 表，检查：

- 文件大小需要的簇数是否能被 FAT 链满足。
- FAT 中是否有已使用但没有目录项引用的丢失簇。
- 是否存在多个文件引用同一簇的交叉链接。

演示流程：

1. 创建干净文件系统并运行 fsck。
2. 注入测试错误：链过早中断、丢失簇、交叉链接。
3. 再次运行 fsck，输出错误报告。

完整输出见：

```text
Assignment4/4.4/output.txt
```

## 6. 结论

Assignment4 在基础 FAT16 上进一步扩展了交互输入、目录层级、大文件优化和一致性检查能力。四个小任务互相独立，便于单独构建、运行和检查；每个小任务都生成自己的 `output.txt`，不会依赖 `Assignment4/output.txt`。
