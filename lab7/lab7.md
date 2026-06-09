# <center>Lab7 内存管理</center>

**本次实验部分代码和注释参考自大模型。**

> 实验环境：Ubuntu 22.04 (x86_64 / WSL2)

## Assignment1 — 物理页内存管理与分页机制

### 1.1 复现物理页内存管理

复现 `src/3` 的物理页内存管理代码，实现 BitMap → AddressPool → MemoryManager 三层管理结构。

#### 三层结构概述

物理页内存管理采用由下至上的三层结构设计：

- **BitMap（位图层）**：最底层的数据结构，用 1 位表示 1 个资源单元（物理页）的状态，1=已分配，0=空闲。提供了按位存取、连续分配和释放的基本操作。
- **AddressPool（地址池层）**：在 BitMap 的基础上加入地址转换逻辑，将 BitMap 的序号转换为实际的物理地址（`startAddress + index × PAGE_SIZE`）。
- **MemoryManager（内存管理层）**：管理内核物理地址池和用户物理地址池，提供 init / alloc / release 接口给上层调用。

这种分层设计的好处是职责清晰——BitMap 只关心位的操作，AddressPool 只负责地址映射，MemoryManager 统筹全局。

#### BitMap 实现

位图是资源管理的基础。对于 4GB 内存、4KB 页的粒度，位图仅需 `4GB / (8 × 4KB) = 128KB`，内存占比约 0.003%，空间效率极高。

```cpp
class BitMap
{
public:
    int length;        // 被管理的资源个数
    char *bitmap;      // 位图存储区域
public:
    BitMap();
    void initialize(char *bitmap, const int length);
    bool get(const int index) const;
    void set(const int index, const bool status);
    int allocate(const int count);    // 分配连续资源
    void release(const int index, const int count);
};
```

`get` 和 `set` 的核心是通过 `index / 8` 定位字节，通过 `index % 8` 定位位偏移：

```cpp
bool BitMap::get(const int index) const
{
    int pos = index / 8;
    int offset = index % 8;
    return (bitmap[pos] & (1 << offset));
}

void BitMap::set(const int index, const bool status)
{
    int pos = index / 8;
    int offset = index % 8;
    bitmap[pos] = bitmap[pos] & (~(1 << offset));  // 清0
    if (status) {
        bitmap[pos] = bitmap[pos] | (1 << offset);  // 置1
    }
}
```

`allocate` 采用顺序扫描，找到第一个能容纳 `count` 个连续空闲资源的块：

```cpp
int BitMap::allocate(const int count)
{
    if (count == 0) return -1;
    int index, empty, start;
    index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;   // 跳过已分配
        if (index == length) return -1;                  // 不够了
        empty = 0;  start = index;
        while ((index < length) && (!get(index)) && (empty < count)) {
            ++empty;  ++index;
        }
        if (empty == count) {  // 找到连续 count 个空闲位
            for (int i = 0; i < count; ++i) set(start + i, true);
            return start;
        }
    }
    return -1;
}
```

#### AddressPool 实现

AddressPool 在 BitMap 的基础上包装了地址计算：

```cpp
class AddressPool
{
public:
    BitMap resources;
    int startAddress;   // 地址池管理的起始物理地址
public:
    void initialize(char *bitmap, const int length, const int startAddress);
    int allocate(const int count);      // 返回分配的物理地址
    void release(const int address, const int amount);
};
```

核心转换逻辑在分配和释放函数中：

```cpp
// 分配：BitMap 返回序号 → 转换为物理地址
int AddressPool::allocate(const int count)
{
    int start = resources.allocate(count);
    return (start == -1) ? -1 : (start * PAGE_SIZE + startAddress);
}

// 释放：物理地址 → 反向计算 BitMap 序号
void AddressPool::release(const int address, const int amount)
{
    resources.release((address - startAddress) / PAGE_SIZE, amount);
}
```

#### MemoryManager 实现

MemoryManager 统筹两个物理地址池和一个内核虚拟地址池。`initialize()` 负责内存布局规划：

```cpp
void MemoryManager::initialize()
{
    this->totalMemory = getTotalMemory();  // 从 0x7c00 读内存大小

    // 预留：1MB 内核空间 + 256 个页表（页目录表 + 255 个页表）
    int usedMemory = 256 * PAGE_SIZE + 0x100000;
    int freeMemory = this->totalMemory - usedMemory;
    int freePages = freeMemory / PAGE_SIZE;
    int kernelPages = freePages / 2;       // 内核/用户等分
    int userPages = freePages - kernelPages;

    int kernelPhysicalStartAddress = usedMemory;
    int userPhysicalStartAddress = usedMemory + kernelPages * PAGE_SIZE;

    // 位图连续放置在 0x10000（BITMAP_START_ADDRESS）处
    int kernelPhysicalBitMapStart = BITMAP_START_ADDRESS;
    int userPhysicalBitMapStart = kernelPhysicalBitMapStart + ceil(kernelPages, 8);

    kernelPhysical.initialize((char *)kernelPhysicalBitMapStart, kernelPages,
                              kernelPhysicalStartAddress);
    userPhysical.initialize((char *)userPhysicalBitMapStart, userPages,
                            userPhysicalStartAddress);
}
```

分配和释放函数根据 `AddressPoolType` 选择对应的地址池：

```cpp
int MemoryManager::allocatePhysicalPages(enum AddressPoolType type, const int count)
{
    int start = -1;
    if (type == AddressPoolType::KERNEL)
        start = kernelPhysical.allocate(count);
    else if (type == AddressPoolType::USER)
        start = userPhysical.allocate(count);
    return (start == -1) ? 0 : start;
}
```

#### 测试设计与运行结果

在 `first_thread` 中设计了两个测试：

**Test 1：基础分配**——从内核池和用户池分别分配 10、20、50 页，打印起始地址并验证地址连续性。

![image-20260608111343819](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608111343819.png)

![image-20260608113246132](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608113246132.png)

**Test 2：释放与复用**——分配 A(10)、B(10)、C(10) 三批页，释放中间的 B，再分配 D(10) 观察是否复用 B 的地址。

![image-20260608113346495](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608113346495.png)

#### 结果分析

从输出截图可以看到：

1. 内核池分配的首地址为 `0x200000`（即 `usedMemory`），用户池首地址紧跟在 `kernelPages × 4KB` 之后，符合 `initialize` 中的布局规划。
2. 地址连续性验证全部 PASS：`k2 - k1 = 0xA000`（10 页），`k3 - k2 = 0x14000`（20 页），说明同池分配的物理页地址是连续的。
3. 释放 B 后再分配 D，D 的地址与 B 完全相同，说明 BitMap 的 release/allocate 正确执行了"释放后空间被复用"的逻辑。

三层结构的数据流为：`MemoryManager.allocatePhysicalPages()` → `AddressPool.allocate()` → `BitMap.allocate()` 返回序号 → 乘以 `PAGE_SIZE` 加 `startAddress` 得到物理地址。释放则是逆过程。这种分层设计使得每一层都专注于单一职责，易于扩展和调试。

---

### 1.2 开启二级分页机制

复现 `src/4` 的代码，开启二级分页机制。

![image-20260608200319966](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608200319966.png)

#### (a) 开启分页机制的三步方案

开启分页机制的核心代码在 `MemoryManager::openPageMechanism()` 中实现，分为三步：

**第一步：初始化页目录表和页表**

```cpp
void MemoryManager::openPageMechanism()
{
    // 页目录表放在 0x100000 处，页表紧跟在后面
    int *directory = (int *)PAGE_DIRECTORY;           // 0x100000
    int *page = (int *)(PAGE_DIRECTORY + PAGE_SIZE);  // 0x101000

    memset(directory, 0, PAGE_SIZE);  // 清零页目录表
    memset(page, 0, PAGE_SIZE);       // 清零第一个页表

    // 建立 0~1MB 的恒等映射：虚拟地址 == 物理地址
    int address = 0;
    for (int i = 0; i < 256; ++i)  // 256 个页表项覆盖 1MB
    {
        page[i] = address | 0x7;   // 0x7 = U/S=1, R/W=1, P=1
        address += PAGE_SIZE;
    }

    // 设置三个关键的页目录项
    directory[0] = ((int)page) | 0x07;     // 0~1MB 恒等映射
    directory[768] = directory[0];           // 3GB 内核空间
    directory[1023] = ((int)directory) | 0x7; // 自引用：PDE 指向页目录表本身

    asm_init_page_reg(directory);  // 第二步+第三步
}
```

页目录表存放在物理地址 `0x100000` 处，其后的 `0x101000` 存放第一个页表。页表项的内容由 `address | 0x7` 构成，其中低 3 位分别表示 P（存在位）、R/W（可读写）、U/S（用户/管理员均可访问）。

**第二步：将页目录表地址写入 CR3**

**第三步：将 CR0 的 PG 位置 1**

第二步和第三步由汇编函数 `asm_init_page_reg` 完成：

```assembly
asm_init_page_reg:
    push ebp
    mov ebp, esp
    push eax

    mov eax, [ebp + 4 * 2]  ; 取出参数（页目录表物理地址）
    mov cr3, eax            ; ★ 第二步：将页目录表地址写入 CR3
    mov eax, cr0
    or eax, 0x80000000      ; ★ 第三步：CR0.PG = 1
    mov cr0, eax

    pop eax
    pop ebp
    ret
```

CR3 是页目录基址寄存器（PDBR），CPU 通过它找到页目录表。CR0 的 PG 位（第 31 位）是分页机制的"总开关"——置 1 后，CPU 自动将虚拟地址通过二级页表转换为物理地址。

> **关键点**：分页机制开启后，程序使用的地址全部变为虚拟地址。我们需要通过 `directory[1023]` 自引用来构造 PDE 和 PTE 的虚拟地址，才能修改页表内容。

#### (b) 三个页目录项的作用分析

| 页目录项 | 指向内容 | 作用 |
|---------|---------|------|
| `directory[0]` | 第一个页表（0x101000） | 将虚拟地址 0~1MB 恒等映射到物理地址 0~1MB，保证开启分页后内核代码能正常运行 |
| `directory[768]` | 与 `directory[0]` 相同 | 将内核空间映射到 3GB（0xC0000000）处，使得所有进程在 3~4GB 范围内共享内核空间 |
| `directory[1023]` | 页目录表自身（0x100000） | 自引用（Self-Referencing），允许通过虚拟地址访问页目录项和页表项，是实现 PDE/PTE 虚拟地址构造的关键 |

**为什么这样设置？**

- `directory[0]` 的恒等映射保证了从实模式到分页模式的平滑过渡——开启分页瞬间，PC 寄存器指向的指令地址仍然可寻址。
- `directory[768]` 与 `directory[0]` 相同，使得虚拟地址 `0xC0000000~0xC0100000` 映射到物理地址 `0x00000000~0x00100000`。这样，无论是通过低地址还是 3GB+ 的高地址，都能访问到内核。在分页机制下，内核通常运行在高地址（3GB+），而物理页分配使用低地址。
- `directory[1023]` 自引用将页目录表的物理地址 0x100000 映射到虚拟地址 `0xFFFFF000`（1023×4MB）。利用这个映射，我们可以通过虚拟地址 `0xFFFFF000 + index × 4` 访问任意页目录项，通过 `0xFFC00000 + higher × 4KB + lower × 4` 访问任意页表项。这部分的具体推导在 3.2 节详述。

#### (c) QEMU Monitor 验证

使用 QEMU Monitor 的 `info mem` 命令查看分页开启后的虚拟地址映射关系：

![image-20260608213311921](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608213311921.png)

`info mem` 显示的映射关系验证了：
- 虚拟地址 `0x00000000~0x000FFFFF`（共 256 页，1MB）映射到物理地址 `0x00000000~0x000FFFFF`，恒等映射正确建立。
- 虚拟地址 `0xC0000000~0xC00FFFFF`（3GB 起始，256 页，1MB）同样映射到物理地址 `0x00000000~0x000FFFFF`，说明 `directory[768]` 正确复制了 `directory[0]` 的映射关系。

---

## Assignment2 — 动态分区分配算法

### 2.1 实现四种分配算法

在 BitMap 层引入了策略模式（Strategy Pattern），通过 `AllocationStrategy` 枚举选择四种不同的分配算法：

```cpp
enum AllocationStrategy { FIRST_FIT, BEST_FIT, WORST_FIT, NEXT_FIT };
```

每个 BitMap 实例持有一个 `strategy` 字段和一个 `lastIndex` 字段（Next Fit 专用）。`allocate()` 函数根据策略分发到对应的私有方法：

```cpp
int BitMap::allocate(const int count)
{
    switch (strategy) {
    case FIRST_FIT:  return allocateFirstFit(count);
    case BEST_FIT:   return allocateBestFit(count);
    case WORST_FIT:  return allocateWorstFit(count);
    case NEXT_FIT:   return allocateNextFit(count);
    default:         return allocateFirstFit(count);
    }
}
```

四种算法在 `setStrategy()` 切换时均重置 `lastIndex = 0`。

#### 四种算法的核心实现

**First Fit（首次适应）**——从头扫描，取第一个足够大的连续空闲块：

```cpp
int BitMap::allocateFirstFit(const int count)
{
    int index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;  // 跳过已分配
        if (index == length) return -1;
        int empty = 0, start = index;
        while ((index < length) && (!get(index)) && (empty < count))
            { ++empty; ++index; }
        if (empty == count) {
            for (int i = 0; i < count; ++i) set(start + i, true);
            return start;
        }
    }
    return -1;
}
```

复杂度 O(n)，每次从 0 开始搜索，在内存前端产生较多小碎片。

**Best Fit（最佳适应）**——扫描所有空闲块，选择最小的能满足需求的块：

```cpp
int BitMap::allocateBestFit(const int count)
{
    int bestStart = -1, bestSize = length + 1, index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;
        if (index == length) break;
        int start = index, free = 0;
        while (index < length && !get(index)) { ++free; ++index; }
        if (free >= count && free < bestSize) {
            bestSize = free;  bestStart = start;
        }
    }
    if (bestStart == -1) return -1;
    for (int i = 0; i < count; ++i) set(bestStart + i, true);
    return bestStart;
}
```

复杂度 O(n)，必须扫描完所有空闲块才能确定"最佳"。能保留大块，但会产生很多极小的碎片。

**Worst Fit（最坏适应）**——扫描所有空闲块，选择最大的块：

```cpp
int BitMap::allocateWorstFit(const int count)
{
    int worstStart = -1, worstSize = -1, index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;
        if (index == length) break;
        int start = index, free = 0;
        while (index < length && !get(index)) { ++free; ++index; }
        if (free >= count && free > worstSize) {
            worstSize = free;  worstStart = start;
        }
    }
    if (worstStart == -1) return -1;
    for (int i = 0; i < count; ++i) set(worstStart + i, true);
    return worstStart;
}
```

复杂度 O(n)，从最大的空闲块中切分，留下的剩余部分可能仍然较大，有利于减少小碎片。

**Next Fit（循环首次适应）**——从 `lastIndex` 开始搜索，到末尾后回绕到 0：

```cpp
int BitMap::allocateNextFit(const int count)
{
    // 第一轮：从 lastIndex 扫描到 length-1
    int i = lastIndex;
    while (i < length) {
        while (i < length && get(i)) ++i;
        if (i == length) break;
        int empty = 0, start = i;
        while ((i < length) && (!get(i)) && (empty < count))
            { ++empty; ++i; }
        if (empty == count) {
            for (int j = 0; j < count; ++j) set(start + j, true);
            lastIndex = start + count;  return start;
        }
    }
    // 第二轮：从 0 扫描到 lastIndex-1
    i = 0;
    while (i < lastIndex) { /* 逻辑相同 */ }
    return -1;
}
```

复杂度 O(n)，但不会每次都从 0 开始，将分配压力分散到整个内存空间。

#### 测试设计与运行结果

测试序列：先分配 4 块（30 页、10 页、20 页、5 页）→ 释放第 1 块（30 页）和第 3 块（20 页）→ 再分配 8 页，观察四种算法选择的位置差异。

**制造的空闲空洞：**
- 空洞 1：索引 [0, 29]，大小 30（释放 30 所得）
- 空洞 2：索引 [40, 59]，大小 20（释放 20 所得）
- 空洞 3：索引 [65, ∞)，剩余大块空间

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
 Next Fit  | 65             | 65 (lastIdx)
===================================
```

#### 算法对比分析

| 算法 | 选择位置 | 选择原因 | 碎片特征 | 时间复杂度 | 适用场景 |
|------|---------|---------|---------|-----------|---------|
| **First Fit** | `idx=0` | 第一个足够大的空闲块 | 内存前端碎片多 | O(n) | 最通用，兼顾速度和空间 |
| **Best Fit** | `idx=40` | 大小 20 是满足 8 的最小块 | 小碎片极多（外部碎片严重） | O(n) | 需保留大块连续空间时 |
| **Worst Fit** | `idx=65` | 最大的空闲块 | 碎片较少，剩余块仍较大 | O(n) | 希望减少小碎片时 |
| **Next Fit** | `idx=65` | 从上次结束位置(65)开始搜索 | 碎片分布均衡 | O(n) | 避免内存前端碎片的场景 |

四种算法在`cleanup()`后（释放所有页）都回归到 `Frag=1, MaxFree=15984`，说明释放逻辑正确。

> **观察结论**：Best Fit 选择了最小的合适空洞（40），保留了更大的空洞给后续大分配；Worst Fit 选择了最大的空洞（65），分配后剩余块（65+8→73）仍较大；Next Fit 因 `lastIndex=65` 从 65 开始搜索，行为与 Worst Fit 在此场景相同；First Fit 直接选择第一个空洞（0），最快但碎片局部化。

---

### 2.2 内存利用率分析

设计了三个统计函数，在每个分配/释放操作后实时监控内存状态：

```cpp
int BitMap::getUsedCount() const
{
    int count = 0;
    for (int i = 0; i < length; ++i)
        if (get(i)) ++count;
    return count;
}

int BitMap::getMaxFreeBlock() const
{
    int maxBlock = 0, index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;
        if (index == length) break;
        int free = 0;
        while (index < length && !get(index)) { ++free; ++index; }
        if (free > maxBlock) maxBlock = free;
    }
    return maxBlock;
}

int BitMap::getFreeFragmentCount() const
{
    int fragments = 0, index = 0;
    while (index < length) {
        while (index < length && get(index)) ++index;
        if (index == length) break;
        ++fragments;  // 找到一个空闲段
        while (index < length && !get(index)) ++index;
    }
    return fragments;
}
```

测试序列为 10 次分配 + 5 次释放，共 15 步操作：

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | `+20` | 分配 20 页 |
| 2 | `+10` | 分配 10 页 |
| 3 | `+30` | 分配 30 页 |
| 4 | `+5` | 分配 5 页 |
| 5 | `-20` | 释放 20 页 |
| 6 | `+15` | 分配 15 页 |
| 7 | `-30` | 释放 30 页 |
| 8 | `+8` | 分配 8 页 |
| 9 | `-10` | 释放 10 页 |
| 10 | `+12` | 分配 12 页 |
| 11 | `-5` | 释放 5 页 |
| 12 | `+7` | 分配 7 页 |
| 13 | `+25` | 分配 25 页 |
| 14 | `-15` | 释放 15 页 |
| 15 | `+3` | 分配 3 页 |

```
===== Lab 2.2: Memory Utilization =====
10 alloc + 5 free: First Fit vs Best Fit
[FF] =====
[FF] Step   1  2  3  4  5  6  7  8  9 10  11 12 13 14 15
[FF] Op   +20 +10 +30  +5  -20 +15 -30  +8 -10 +12  -5  +7 +25 -15  +3
[FF] Used  20 30 60 65 45 60 30 38 28 40  35 42 67 52 55
[FF] Frag  1 1 1 1 2 2 3 2 3 2  1 1 1 2 2
[FF] MaxFree 15964 15954 15924 15919 15919 15919 15919 15919 15919 15919 15949 15942 15917 15917 15917
[BF] =====
[BF] Step   1  2  3  4  5  6  7  8  9 10  11 12 13 14 15
[BF] Op   +20 +10 +30  +5  -20 +15 -30  +8 -10 +12  -5  +7 +25 -15  +3
[BF] Used  20 30 60 65 45 60 30 38 28 40  35 42 67 52 55
[BF] Frag  1 1 1 1 2 2 3 2 3 2  1 1 1 2 2
[BF] MaxFree 15964 15954 15924 15919 15919 15919 15919 15919 15919 15919 15949 15942 15917 15917 15917
```

#### 对比分析

从输出数据可以看出：

**相同点**：First Fit 和 Best Fit 在碎片数量（Frag）上完全相同。这是因为释放操作创造的空洞位置在两个算法中是相同的，而碎片计数只统计不连续的空闲段个数，不关心空洞的位置。

**关键差异**：虽然在 15 步序列中 Frag 和 Used 完全相同，但**实际分配的地址位置不同**。在 2.1 的测试中我们可以清楚看到，当存在多个空洞时：
- First Fit 选择第一个空洞（索引 0）
- Best Fit 选择最小的合适空洞（索引 40）
- Worst Fit / Next Fit 选择最大的空洞（索引 65）

**碎片产生的根本原因**：分配和释放交替进行时，中间的空闲区域被切割成多个不连续的片段。从输出中可以看到，Frag 在第 7 步释放 30 页后达到峰值 3，随后分配 8 页后因填补了碎片而降为 2。这说明碎片数量随着分配/释放的进行动态波动。

> **结论**：在碎片数量指标上，First Fit 和 Best Fit 在相同操作序列下表现一致，因为碎片数量只与空闲块的个数有关，与选择哪个空洞无关。但在实际分配位置和保留连续空间的能力上，Best Fit 更能保留大块空闲区域。四种算法的本质差异在于空间分配策略对碎片分布的影响模式不同。

---

## Assignment3 — 虚拟页内存管理与地址变换

### 3.1 复现虚拟页内存管理

复现 `src/5` 的代码，实现虚拟页内存管理。

![image-20260608215635250](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260608215635250.png)

#### (a) 虚拟页内存分配的三步过程

虚拟页内存分配的核心函数是 `allocatePages()`，分为严格的三步：

```cpp
int MemoryManager::allocatePages(enum AddressPoolType type, const int count)
{
    // ★ 第一步：从虚拟地址池分配连续虚拟页
    int virtualAddress = allocateVirtualPages(type, count);
    if (!virtualAddress) return 0;

    bool flag;
    int physicalPageAddress;
    int vaddress = virtualAddress;

    // ★ 第二步+第三步：依次为每个虚拟页分配物理页并建立映射
    for (int i = 0; i < count; ++i, vaddress += PAGE_SIZE)
    {
        flag = false;
        // 第二步：从物理地址池分配一个物理页
        physicalPageAddress = allocatePhysicalPages(type, 1);
        if (physicalPageAddress)
        {
            // 第三步：建立虚拟页→物理页的 PDE/PTE 映射
            flag = connectPhysicalVirtualPage(vaddress, physicalPageAddress);
        }

        // 分配失败处理：释放之前已分配的资源
        if (!flag) {
            releasePages(type, virtualAddress, i);           // 释放物理页
            releaseVirtualPages(type, virtualAddress + i * PAGE_SIZE, count - i);
            return 0;
        }
    }
    return virtualAddress;
}
```

**第一步：从虚拟地址池分配连续虚拟页**——调用 `allocateVirtualPages()` 从内核虚拟地址池分配连续的虚拟页：

```cpp
int MemoryManager::allocateVirtualPages(enum AddressPoolType type, const int count)
{
    int start = -1;
    if (type == AddressPoolType::KERNEL)
        start = kernelVirtual.allocate(count);
    return (start == -1) ? 0 : start;
}
```

内核虚拟地址从 `KERNEL_VIRTUAL_START = 0xC0100000` 开始，共 `kernelPages` 个页。

**第二步：从物理地址池分配物理页**——与 1.1 相同的 `allocatePhysicalPages()`。

**第三步：建立页目录项和页表项**——这是分页机制的核心，通过 `connectPhysicalVirtualPage()` 实现：

```cpp
bool MemoryManager::connectPhysicalVirtualPage(const int virtualAddress,
                                                const int physicalPageAddress)
{
    // 计算虚拟地址对应的 PDE 和 PTE 的虚拟地址
    int *pde = (int *)toPDE(virtualAddress);
    int *pte = (int *)toPTE(virtualAddress);

    // 若 PDE 不存在（P 位为 0），先分配一个物理页作为页表
    if (!(*pde & 0x00000001))
    {
        int page = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
        if (!page) return false;
        *pde = page | 0x7;  // 页表物理地址 | U/S=1 R/W=1 P=1
        // 初始化新页表（清空所有页表项）
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }

    // 设置页表项：物理页地址 | 属性位
    *pte = physicalPageAddress | 0x7;
    return true;
}
```

这里的关键是 `toPDE()` 和 `toPTE()` 两个函数，它们利用 `directory[1023]` 的自引用来构造页目录项和页表项的虚拟地址：

```cpp
// 构造 PDE 的虚拟地址
int MemoryManager::toPDE(const int virtualAddress)
{
    return (0xfffff000 + (((virtualAddress & 0xffc00000) >> 22) * 4));
}

// 构造 PTE 的虚拟地址
int MemoryManager::toPTE(const int virtualAddress)
{
    return (0xffc00000 + ((virtualAddress & 0xffc00000) >> 10)
            + (((virtualAddress & 0x003ff000) >> 12) * 4));
}
```

**PDE 构造原理**：`directory[1023]` 指向页目录表本身，因此页目录表被映射到 `0xFFFFF000`（`1023 × 4MB + 0`）。第 `i` 个 PDE 的虚拟地址 = `0xFFFFF000 + i × 4`。

**PTE 构造原理**：利用自引用 PDE 访问页表，`0xFFC00000 + dir_index × 4KB + table_index × 4`，其中 `dir_index = virtualAddress[31:22]`，`table_index = virtualAddress[21:12]`。

#### (b) 测试设计与运行结果

在 `first_thread` 中设计 6 步测试序列：

1. 分配 100 页 → 2. 分配 10 页 → 3. 分配 100 页 → 4. 释放中间的 10 页 → 5. 分配 100 页 → 6. 分配 10 页

```
=== 3.1 Alloc/Release Test ===
#1 +100 v=C0100000 PDE=FFFFFC00 PTE=FFF00400 ph=200000
#2  +10 v=C0164000 PDE=FFFFFC00 PTE=FFF00590 ph=264000
#3 +100 v=C016E000 PDE=FFFFFC00 PTE=FFF005B8 ph=26E000
#4  -10 @C0164000 (released)
#5 +100 v=C01D2000 PDE=FFFFFC00 PTE=FFF00748 ph=264000
#6  +10 v=C0164000 PDE=FFFFFC00 PTE=FFF00590 ph=32C000
```

#### 结果分析

从输出可以观察到以下规律：

1. **虚拟地址连续增长**：`v=C0100000 → C0164000 → C016E000 → C01D2000 → C0164000`。内核虚拟地址池从 `0xC0100000` 开始向上分配，每次分配后的地址紧接着上一次的末尾。
2. **释放后地址复用**：`#4 释放 @C0164000`（10 页）后，`#6 再次分配 10 页` 时虚拟地址 **回到 `C0164000`**，说明虚拟地址池正确复用了被释放的虚拟地址。
3. **物理地址可以不连续**：观察 `#2 ph=264000` 和 `#6 ph=32C000`，虽然虚拟地址都是 `C0164000`，但物理地址不同（264000 和 32C000），因为 `#4` 释放物理页后 `#6` 重新分配时物理地址池的布局已变化。
4. **PDE 地址固定**：所有虚拟地址的 PDE 地址都是 `FFFFFC00`，因为它们都属于第 768 个页目录项（内核虚拟空间 `0xC0000000~0xC0100000` 对应 PDE 768，`(0xC0100000 >> 22) & 0xFFF = 768`）。PDE 虚拟地址 = `0xFFFFF000 + 768 × 4 = 0xFFFFF000 + 0xC00 = 0xFFFFFC00`。

---

### 3.2 PDE/PTE 虚拟地址构造

**题目：** 假设将自引用页目录项从第 1023 个改为**第 1000 个**，请推导：
- 第 **141** 个页目录项的虚拟地址
- 第 **891** 个页目录项指向的页表中，第 **109** 个页表项的虚拟地址

#### 推导过程

当自引用 PDE 放在第 1000 个页目录项时，页目录表被映射到虚拟地址 `1000 × 4MB = 0xFA000000`。

> 具体计算：1000 = 0x3E8，`0x3E8 << 22 = 0x3E8 × 0x400000 = 0xFA000000`。

**推导 1：第 141 个页目录项的虚拟地址**

第 `i` 个 PDE 在页目录表中的偏移 = `i × 4`。
页目录表的虚拟地址 = 自引用 PDE 指向的虚拟地址 = `0xFA000000`。

因此，第 141 个 PDE 的虚拟地址 = `0xFA000000 + 141 × 4`。

```
0xFA000000 = 1111 1010 0000 0000 0000 0000 0000 0000
+ 141 × 4  = 0x234                       = 0000 0000 0000 0000 0000 0010 0011 0100
-----------------------------------------------------------------------------
            = 1111 1010 0000 0000 0000 0010 0011 0100
            = 0xFA000234
```

**推导 2：第 891 个 PDE 指向的页表中，第 109 个 PTE 的虚拟地址**

- 第 891 个 PDE 指向的页表的虚拟地址 = `0xFA000000 + 891 × 4 = 0xFA000DEC`（这是 PDE 本身的虚拟地址，但 PDE 的内容是页表的物理地址——我们需要的是页表的虚拟地址）
- 实际上，**页表的虚拟地址**是通过自引用 PDE 构造的：所有页表被映射到 `0xFA000000` 开始的范围内，每个页表占 4KB（即一个页的大小）。
- 第 `dir` 个 PDE 指向的页表，其虚拟地址 = `0xFA000000 + dir × 4KB = 0xFA000000 + dir × 0x1000`。
- 在该页表中，第 `table` 个 PTE 的偏移 = `table × 4`。

因此，第 891 个 PDE 指向的页表中，第 109 个 PTE 的虚拟地址：
```
= 0xFA000000 + 891 × 0x1000 + 109 × 4
= 0xFA000000 + 891 × 4096 + 436
= 0xFA000000 + 0xDEC000 + 0x1B4
= 0xFA000000 + 0xDEC000 + 0x1B4
= 0xFADEC1B4
```

验证（十六进制计算）：
```
0xFA000000
+ 0xDEC000   (891 × 4096 = 891 × 0x1000 = 0xDEC000)
= 0xFADEC000
+ 0x1B4      (109 × 4 = 436 = 0x1B4)
= 0xFADEC1B4
```

#### 最终结果

| 项目 | 结果 |
|------|------|
| 第 141 个页目录项的虚拟地址 | **`0xFA000234`** |
| 第 891 个页目录项指向的页表中，第 109 个页表项的虚拟地址 | **`0xFADEC1B4`** |

---

### 3.3 虚拟地址到物理地址的验证

在 `first_thread` 中分配 1 页内核虚拟内存，调用 `vaddr2paddr` 获取物理地址，写入特定值后通过 QEMU Monitor 的 `xp` 命令验证：

```
=== 3.3 Vaddr to Paddr Verification ===
#1 Allocated vaddr: 0xC0100000
#2 vaddr2paddr -> phy: 0x200000
#3 Wrote 0xDEADBEEF to *(C0100000)=DEADBEEF
    (verify via QEMU Monitor: xp 0x200000)
=== Done (looping for monitor) ===

--- QEMU Monitor xp verification ---
0000000000200000: 0xdeadbeef
```

`vaddr2paddr` 的实现利用了 PTE 虚拟地址来读取页表项中的物理页地址：

```cpp
int MemoryManager::vaddr2paddr(int vaddr)
{
    int *pte = (int *)toPTE(vaddr);   // 获取 PTE 的虚拟地址
    int page = (*pte) & 0xfffff000;   // 取出物理页地址（高 20 位）
    int offset = vaddr & 0xfff;       // 页内偏移（低 12 位）
    return (page + offset);
}
```

分配到的虚拟地址 `0xC0100000` 通过 `vaddr2paddr` 计算得到物理地址 `0x200000`。向虚拟地址写入 `0xDEADBEEF` 后，在 QEMU Monitor 中用 `xp 0x200000` 验证，物理地址处的值确实为 `0xdeadbeef`，证明了虚拟地址到物理地址的映射完全正确。

![image-20260609173028152](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260609173028152.png)

---

## Assignment4 — 进阶实验

### 4.1 Clock 页面置换算法模拟

#### 实现思路

修改 MemoryManager，限制物理页帧总数为 48（`MAX_PHYSICAL_PAGES`），当分配的物理页数达到上限后，**不再从物理地址池分配新页**，而是触发 Clock 页面置换算法选择牺牲页，将新虚拟页映射到被淘汰的物理页上。

关键数据结构（内联数组成员，避免地址冲突）：

```cpp
class MemoryManager {
    // ...
    int maxPhysicalPages;           // = 48
    int physicalSlots[48];          // 每个 slot 存储的物理页地址
    int slotVA[48];                 // 每个 slot 对应的虚拟地址
    int pageAccessBits[48];         // Clock 算法的访问位
    int clockHand;                  // Clock 指针
    int allocatedPhysicalCount;     // 已分配的数据页数
};
```

#### 核心算法实现

**`accessPage(int vaddr)`**：模拟对虚拟页的访问操作，将对应 slot 的访问位置 1：

```cpp
void MemoryManager::accessPage(int vaddr)
{
    for (int i = 0; i < allocatedPhysicalCount; ++i)
    {
        if (slotVA[i] == vaddr)
        {
            pageAccessBits[i] = 1;  // 标记为已访问（给予"第二次机会"）
            break;
        }
    }
}
```

**`selectVictimPage()`**：Clock 算法的核心——循环扫描，访问位为 0 则选中，为 1 则清 0 后继续：

```cpp
int MemoryManager::selectVictimPage()
{
    int total = allocatedPhysicalCount;
    while (true)
    {
        for (int i = 0; i < total; ++i)
        {
            int idx = (clockHand + i) % MAX_PHYSICAL_PAGES;
            if (physicalSlots[idx] == 0) continue;  // 跳过空 slot

            if (pageAccessBits[idx] == 0)
            {
                // 访问位为 0 → 选中为牺牲页
                clockHand = (idx + 1) % MAX_PHYSICAL_PAGES;
                return idx;
            }
            else
            {
                // → 给予第二次机会，清 0 后继续扫描
                pageAccessBits[idx] = 0;
            }
        }
        clockHand = (clockHand + 1) % MAX_PHYSICAL_PAGES;
    }
}
```

**`pageReplacement(int vaddr)`**：执行置换——淘汰旧页，映射新页：

```cpp
void MemoryManager::pageReplacement(int vaddr)
{
    int idx = selectVictimPage();
    int victimPA = physicalSlots[idx];
    int victimVA = slotVA[idx];

    printf("[PageReplacement] Victim: VA=0x%x PA=0x%x → New VA=0x%x\n",
           victimVA, victimPA, vaddr);

    // 清除旧页的 PTE（页表项置 0）
    int *victimPte = (int *)toPTE(victimVA);
    *victimPte = 0;

    // 释放物理页 + 清理 slot 记录
    releasePhysicalPages(AddressPoolType::KERNEL, victimPA, 1);
    physicalSlots[idx] = 0;
    slotVA[idx] = 0;
    pageAccessBits[idx] = 0;

    // 重新分配物理页（优先使用刚释放的页）
    int newPA = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
    // 建立新映射
    connectPhysicalVirtualPage(vaddr, newPA);
    physicalSlots[idx] = newPA;
    slotVA[idx] = vaddr;
}
```

**修改 `allocatePages()` 触发置换**：当 `allocatedPhysicalCount >= maxPhysicalPages` 时，不再继续从池分配，而是调用 `pageReplacement()`：

```cpp
// 在 allocatePages() 的循环内
if (allocatedPhysicalCount >= maxPhysicalPages)
{
    pageReplacement(vaddress);  // 走置换路径
    flag = true;
}
else
{
    physicalPageAddress = allocatePhysicalPages(type, 1);
    if (physicalPageAddress) {
        flag = connectPhysicalVirtualPage(vaddress, physicalPageAddress);
        physicalSlots[allocatedPhysicalCount] = physicalPageAddress;
        slotVA[allocatedPhysicalCount] = vaddress;
        allocatedPhysicalCount++;
    }
}
```

#### 测试设计与运行结果

测试设计：Phase 1 分配 45 页（< 48，不到上限），Phase 2 再分配 15 页，触发 12 次置换。

> **注意**：实验截图和详细输出日志已随代码一并提交，此处省略中间输出。通过观察置换日志可以看到：
> - 前 45 次分配正常进行，不触发置换
> - 第 46~60 次分配触发 12 次 Clock 置换
> - 访问位为 1 的页面获得"第二次机会"不被立即淘汰
> - 每次置换正确输出被淘汰的旧 VA、旧 PA 和新分配的 VA

**Clock 算法特性分析**：Clock 算法是对 FIFO 的改进——通过访问位给予活跃页面"第二次机会"，避免频繁置换正在使用的页面。相比 FIFO，Clock 不会出现 Belady 异常（分配的物理页越多缺页率反而升高的问题）；相比 LRU，Clock 只需维护 1 位访问位而非完整的访问历史，硬件开销更小，因此在现代操作系统中被广泛采用（Linux 内核的近似 LRU 算法即是基于 Clock 的变体）。

---

### 4.2 内存统计面板

实现 `printMemoryStatus()` 函数，在屏幕上显示完整的内存使用状况。

**BitMap 扩展**：新增 `countAllocated()` 方法统计已分配的位数：

```cpp
int BitMap::countAllocated() const
{
    int count = 0;
    for (int i = 0; i < length; ++i)
    {
        if (get(i)) ++count;
    }
    return count;
}
```

**`printMemoryStatus()` 核心实现**：

```cpp
void MemoryManager::printMemoryStatus()
{
    // 1. 物理内存总量
    int totalKB = totalMemory / 1024;
    printf("=== Physical Memory: %d KB (%d MB) ===\n", totalKB, totalKB / 1024);

    // 2. 内核物理地址池统计
    int kUsed = kernelPhysical.resources.countAllocated();
    int kTotal = kernelPhysical.resources.size();
    printf("[Kernel Phys] %d/%d pages used (%d%%), free: %d\n",
           kUsed, kTotal, kUsed * 100 / kTotal, kTotal - kUsed);

    // 3. 用户物理地址池统计
    int uUsed = userPhysical.resources.countAllocated();
    int uTotal = userPhysical.resources.size();
    printf("[User Phys]   %d/%d pages used (%d%%), free: %d\n",
           uUsed, uTotal, uUsed * 100 / uTotal, uTotal - uUsed);

    // 4. 内核虚拟地址池统计
    int kvUsed = kernelVirtual.resources.countAllocated();
    int kvTotal = kernelVirtual.resources.size();
    printf("[Kernel Virt] %d/%d pages used (%d%%), free: %d\n",
           kvUsed, kvTotal, kvUsed * 100 / kvTotal, kvTotal - kvUsed);

    // 5. 页目录项统计（遍历 PDE 表，统计 P=1 的项）
    int *pdeTable = (int *)0xFFFFF000;  // 自引用 PDE
    int pdeCount = 0;
    for (int i = 0; i < 1024; ++i) {
        if (pdeTable[i] & 0x1) ++pdeCount;  // P=1
    }
    printf("[PDE] %d/1024 entries active\n", pdeCount);

    // 6. 页表项统计（遍历所有 PTE，统计 P=1 的项）
    int pteCount = 0;
    for (int pdeIdx = 0; pdeIdx < 1024; ++pdeIdx) {
        if (!(pdeTable[pdeIdx] & 0x1)) continue;  // PDE 不存在，跳过
        int *pteTable = (int *)(0xFFC00000 + pdeIdx * 0x1000);
        for (int pteIdx = 0; pteIdx < 1024; ++pteIdx) {
            if (pteTable[pteIdx] & 0x1) ++pteCount;
        }
    }
    printf("[PTE] %d entries active (mapped pages)\n", pteCount);
}
```

测试流程：
1. 打印初始内存状态
2. 依次分配 100 页 → 10 页 → 100 页 → 释放 10 页 → 再分配 100 页 → 再分配 10 页
3. 每次操作后打印内存状态

> **注意**：串口输出（通过 `-serial stdio` 映射到终端）可以完整捕获所有统计输出。运行结果展示随着页面的分配与释放，三个地址池的使用率动态变化，PDE 和 PTE 的数量也相应增减，完整反映了内存管理状态的变化。

---

### 4.3 Buddy System 内存管理

自行实现 Buddy System 伙伴系统作为替代的物理页管理方案。

#### BuddyAllocator 数据结构

```cpp
#define BUDDY_MAX_ORDER 12      // 最大阶：2^12 = 4096 页

struct BlockNode {
    int addr;              // 块起始地址（物理地址）
    int order;             // 块阶数（2^order 个连续页）
    BlockNode *prev, *next;
};

struct AllocRecord {
    int addr;              // 分配的起始地址
    int order;             // 分配的阶数
    bool used;             // 是否在使用中
};

class BuddyAllocator {
    BlockNode *freeArea[BUDDY_MAX_ORDER + 1];  // 空闲链表数组
    AllocRecord allocRecords[1024];            // 分配追踪表
    int blockPool[2048];                       // 预分配节点池
    int totalPages;     // 总页数
    int allocatedPages; // 已分配页数
    int startAddr;      // 起始物理地址
};
```

Buddy System 的核心思想是将物理页按 2^n 的幂次分组（阶数），每阶维护一个空闲块链表。分配时从需求阶数向上找首个非空链表，逐级分割；释放时反向尝试伙伴合并。

#### 核心算法

**初始化**：将所有空闲页合并为一个最大阶的大块加入 free list：

```cpp
void BuddyAllocator::initialize(int startAddr, int totalPageCount)
{
    this->startAddr = startAddr;
    this->totalPages = totalPageCount;
    this->allocatedPages = 0;

    // 初始化各阶链表为空
    for (int i = 0; i <= BUDDY_MAX_ORDER; ++i)
        freeArea[i] = nullptr;

    // 计算最大阶数
    int maxOrder = 0;
    int pages = totalPageCount;
    while (pages > 1) { pages >>= 1; ++maxOrder; }

    // 将所有空闲页作为一个大块加入最高阶
    addBlock(freeArea[maxOrder], startAddr, maxOrder);
}
```

**分配**：从目标阶向上查找第一个非空链表，取出块后逐级分割：

```cpp
int BuddyAllocator::allocate(int pageCount)
{
    // 计算所需的最小阶（向上取整到 2^n）
    int targetOrder = 0, pages = 1;
    while (pages < pageCount) { pages <<= 1; ++targetOrder; }

    // 从 targetOrder 开始向上找第一个非空 free list
    int order = targetOrder;
    while (order <= BUDDY_MAX_ORDER && freeArea[order] == nullptr)
        ++order;
    if (order > BUDDY_MAX_ORDER) return -1;  // 分配失败

    // 取出该大块
    BlockNode *block = popBlock(freeArea[order]);

    // 逐级分割（Split），高余部分加入低阶 free list
    while (order > targetOrder)
    {
        --order;
        int buddyAddr = block->addr + (1 << order) * PAGE_SIZE;
        addBlock(freeArea[order], buddyAddr, order);
    }

    // 记录分配信息
    trackAlloc(block->addr, targetOrder);
    allocatedPages += (1 << targetOrder);
    return block->addr;
}
```

**释放**：计算伙伴地址，尝试逐级合并：

```cpp
void BuddyAllocator::release(int addr)
{
    // 查找分配记录
    int order = findAllocOrder(addr);
    if (order < 0) return;

    allocatedPages -= (1 << order);
    int currentAddr = addr;
    int currentOrder = order;

    // 逐级尝试伙伴合并
    while (currentOrder < BUDDY_MAX_ORDER)
    {
        int buddyAddr = currentAddr ^ ((1 << currentOrder) * PAGE_SIZE);
        BlockNode *buddy = findBlock(freeArea[currentOrder], buddyAddr);
        if (buddy == nullptr) break;  // 伙伴不空闲，停止合并

        // 从 free list 移除伙伴，合并
        removeBlock(freeArea[currentOrder], buddy);
        currentAddr = (currentAddr < buddyAddr) ? currentAddr : buddyAddr;
        ++currentOrder;
    }

    // 将合并后的块加入 free list
    addBlock(freeArea[currentOrder], currentAddr, currentOrder);
}
```

> **伙伴地址计算**：`buddyAddr = addr ^ (blockSize)`。例如 4KB 块的伙伴 = `addr ^ 0x1000`，8KB 块的伙伴 = `addr ^ 0x2000`。这是 Buddy System 最精妙的设计——伙伴地址可通过简单的异或运算得到。

#### 测试设计与运行结果

**Test 1：基本分配**——分配不同大小的块（1 页、2 页、3 页→向上取整到 4 页、8 页），观察 split 行为。

**Test 2：释放与合并**——释放某些块，观察伙伴合并后空闲块阶数提升。

**Test 3：压力测试**——循环分配 1 页直到耗尽，观察最大可分配数量。

> **运行结果摘要**：
> - Test 1 显示分配的地址均为 2^n × 4KB 对齐，体现了 Buddy System 的块对齐特性
> - Test 2 显示释放相邻的块后，它们成功合并为更大的连续块
> - Test 3 展示了系统可分配的最大页数，未出现内存碎片导致的"内存足够但无法连续分配"的问题

#### 与 BitMap 方案的对比分析

| 对比维度 | BitMap 方案 | Buddy System |
|---------|------------|-------------|
| **数据结构** | 位图（1 位/页） | 链表数组（按阶分级） |
| **分配算法** | 顺序扫描 O(n) | 分割查找 O(log N) |
| **外部碎片** | 多（顺序扫描易产生碎片） | 少（2^n 对齐，伙伴合并） |
| **大块分配能力** | 需要连续位，大块易失败 | 向上找高阶块，成功率更高 |
| **空间开销** | 位图 ≈ 0.003% | 链表节点 + 追踪表 ≈ 少量 KB |
| **实现复杂度** | 简单 | 中等 |

**Buddy System 的核心优势**在于：通过 2^n 的分级和伙伴合并机制，有效减少了外部碎片。当释放内存时，相邻的"伙伴"块可以自动合并为更大的块，避免了大块分配时因碎片导致的失败。这在需要频繁分配/释放不同大小内存块的场景下尤为有效。

---

## 遇到的问题及解决方法

### 问题 1：追踪数组地址冲突（4.1）

**现象**：实现 Clock 页面置换时，将 `physicalSlots` 等追踪数组放在地址 `0x11000` 处。运行时发现 `allocatedPhysicalCount` 的值始终是 32 而不是预期的 45，且输出内容混乱。

**原因**：`0x11000` 恰好与 BitMap 区域重叠（`BITMAP_START_ADDRESS = 0x10000`，内核物理池位图约 2000 字节，用户物理池位图紧随其后），追踪数组覆写了位图数据，导致 `allocatePhysicalPages()` 返回错误的分配计数。

**解决**：将追踪数组改为类内联数组成员：

```cpp
int physicalSlots[MAX_PHYSICAL_PAGES];  // 内联数组成员
int slotVA[MAX_PHYSICAL_PAGES];
int pageAccessBits[MAX_PHYSICAL_PAGES];
```

内联数组的内存空间在程序加载时即被分配，与位图区域完全不重叠。修改后 `allocatedPhysicalCount` 正确显示为 45，置换逻辑正常运行。

### 问题 2：串口输出缺失（4.2）

**现象**：4.2 的内存统计面板代码编译运行后，QEMU 终端无任何输出，但代码逻辑正确。

**原因**：`stdio.cpp` 中只实现了 VGA 显存输出，未初始化串口（COM1 端口 `0x3F8`）。在 `print()` 函数中，字符仅写入 VGA 显存，未同步输出到串口。而 Makefile 使用 `-serial stdio` 将串口映射到终端，终端看不到 VGA 输出的内容。

**解决**：在 `STDIO::initialize()` 中添加串口初始化代码：

```cpp
void serial_init()
{
    asm_out_port(0x3F8 + 1, 0x00);  // 关闭中断
    asm_out_port(0x3F8 + 3, 0x80);  // 设置 DLAB=1
    asm_out_port(0x3F8 + 0, 0x0C);  // 波特率低字节 (115200)
    asm_out_port(0x3F8 + 1, 0x00);  // 波特率高字节
    asm_out_port(0x3F8 + 3, 0x03);  // 8位数据，1位停止位
    asm_out_port(0x3F8 + 2, 0xC7);  // FIFO 启用
    asm_out_port(0x3F8 + 4, 0x0B);  // DTR/RTS 开启
}
```

同时在 `print()` 的字符输出分支中调用 `serial_putc(str[i])`，并处理 `\n` 时额外发送 `\r`。修改后输出同步显示在终端，完整捕获了内存统计面板的所有信息。

### 问题 3：Buddy System 析构函数链接错误（4.3）

**现象**：编译 4.3 时，链接器报 `undefined reference to 'BuddyAllocator::~BuddyAllocator()'` 错误。

**原因**：内核运行在 nostdlib 环境下（无标准库），C++ 的析构函数需要 `__cxa_atexit` 等运行时支持，而内核代码中没有提供这些符号。`MemoryManager` 类含有 `BuddyAllocator` 成员，在全局析构时链接器尝试生成析构函数调用，导致链接失败。

**解决**：移除 `BuddyAllocator` 的显式析构函数声明，同时在 Makefile 中添加编译标志：

```makefile
CXX_FLAGS += -fno-exceptions -fno-rtti
```

`-fno-exceptions` 禁用 C++ 异常处理（内核不需要异常机制），`-fno-rtti` 禁用运行时类型识别。这两个标志减少了内核代码对 C++ 运行时库的依赖。移除析构函数后，编译器不再为 BuddyAllocator 生成析构代码，链接通过。

---

## 思考题

### 1. 相比于一级页表，二级页表的好处是什么？在什么情况下二级页表的空间开销反而更大？

**好处**：

- **按需创建页表**：一级页表需要预先分配 1M 个页表项（4MB），而二级页表仅需先分配页目录表（4KB），页表按需创建。对于只使用了少量虚拟地址的进程，页表空间开销大大降低。
- **便于进程隔离**：每个进程有独立的页目录表，切换进程只需切换 CR3，实现进程间地址空间隔离。
- **共享内核空间**：多个进程的页目录表可以共享 768~1023 号页目录项（内核空间），避免了内核页表的重复创建。

**空间开销更大的情况**：当进程使用了几乎所有虚拟地址空间时，二级页表需要页目录表（4KB）+ 1024 个页表（1024 × 4KB）= 4MB + 4KB，比一级页表的 4MB 稍微大一点。但这种情况极为罕见。

### 2. 将 `0xe801` 改为 `0xe820` 需要修改哪些代码？优势是什么？

**需要修改的代码**：

- **MBR/bootloader 中的中断调用**：`0xe801` 只需调用一次，返回结果在 AX/BX 中。`0xe820` 需要循环调用，每次返回一个内存区域描述符（ARDS），需要参数设置（ES:DI 指向缓冲区、ECX 为缓冲区大小、EDX=0x534D4150 作为签名）。
- **内存大小读取函数**：`getTotalMemory()` 需要从缓冲区中解析多个 ARDS 结构体，累加所有可用类型的内存区域，而非简单的 `low × 1024 + high × 64 × 1024`。

**`0xe820` 的优势**：可以获取超过 4GB 的内存容量，且能区分不同的内存区域类型（可用内存、ACPI 保留、硬件保留等），信息更丰富、更精确。

### 3. 多进程环境下如何保证不同进程的虚拟地址空间互不干扰？

每个进程有独立的页目录表和页表。进程切换时，操作系统将目标进程的页目录表物理地址写入 CR3，强制 MMU 使用新进程的地址映射。由于不同进程的页表内容不同，即使虚拟地址相同（如 `0x400000`），在同一时刻只会有一个进程处于运行态，其页表映射的物理地址对其他进程不可见。这样，进程 A 无法访问进程 B 的物理内存，实现了地址空间隔离。

### 4. 请用自己的话说说如何通过自引用页目录项构造 PDE 和 PTE 的虚拟地址

页目录表的第 1023 个页目录项指向页目录表自身（自引用）。这样，页目录表本身也被映射到了一个虚拟地址范围——`0xFFFFF000` ~ `0xFFFFFFFF`（1023 × 4MB ~ 1024 × 4MB）。

- **构造 PDE 虚拟地址**：在 `0xFFFFF000` 开始的范围内，第 `i` 个 PDE 的虚拟地址 = `0xFFFFF000 + i × 4`。CPU 访问这个地址时，自引用机制让它经过两次页表转换后回到页目录表本身。

- **构造 PTE 虚拟地址**：所有页表被映射到 `0xFFC00000` 开始的连续范围内（每个页表占 4KB）。对于虚拟地址 `vaddr`，其 PDE 索引为 `dir = vaddr[31:22]`，PTE 索引为 `table = vaddr[21:12]`。PTE 的虚拟地址 = `0xFFC00000 + dir × 4KB + table × 4`。其中 `0xFFC00000` 是第 1023 个 PDE 指向的页表（即页目录表自身）的虚拟地址，`dir × 4KB` 跳转到第 `dir` 个页表在虚拟空间的偏移，`table × 4` 定位到该页表中的具体页表项。

这种巧妙的构造方法使得在内核代码中可以通过虚拟地址直接访问和修改页表，而无需使用物理地址。
