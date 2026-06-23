# Assignment 2.1 进程的创建过程分析

## 一、实验目标

本实验基于 `Assignment2/2.1`，也就是课程材料中的 `src/3`，复现用户进程创建过程。实验要求在内核线程中创建至少 3 个用户进程，并结合代码分析：

1. 如何在线程 PCB 的基础上扩展进程 PCB。
2. 如何设置 `ProcessStartStack`，使 `asm_start_process` 通过 `iret` 从内核态跳到用户态。
3. 调度时如何切换进程页目录表，即切换 `CR3`。
4. 为什么线程 PCB 的 `pageDirectoryAddress` 为 0，而进程的不为 0。

## 二、实验实现

用户态不能直接访问显存输出，因此 3 个用户进程都通过 0 号系统调用进入内核态打印信息。

在 `src/kernel/setup.cpp` 中定义 3 个用户进程函数：

```cpp
void process_a()
{
    asm_system_call(0, 1, 101, 102, 103, 104);
    asm_halt();
}

void process_b()
{
    asm_system_call(0, 2, 201, 202, 203, 204);
    asm_halt();
}

void process_c()
{
    asm_system_call(0, 3, 301, 302, 303, 304);
    asm_halt();
}
```

然后在第一个内核线程 `first_thread` 中创建 3 个用户进程：

```cpp
void first_thread(void *arg)
{
    printf("start process\n");
    programManager.executeProcess((const char *)process_a, 1);
    programManager.executeProcess((const char *)process_b, 1);
    programManager.executeProcess((const char *)process_c, 1);
    asm_halt();
}
```

这里没有直接让用户进程调用 `printf`，因为用户进程运行在 CPL=3，不能直接使用内核态的显存访问能力。通过 `asm_system_call` 执行 `int 0x80` 后，CPU 会进入内核态，再由系统调用处理函数执行 `syscall_0` 完成输出。

## 三、运行方法和结果

编译：

```bash
cd Assignment2/2.1/build
make clean && make build
```

运行：

```bash
make run
```

预期可以看到类似输出：

```text
start process
systerm call 0: 1, 101, 102, 103, 104
systerm call 0: 2, 201, 202, 203, 204
systerm call 0: 3, 301, 302, 303, 304
```

由于调度器和时钟中断会影响进程运行时机，3 个用户进程输出的顺序不要求固定。只要 3 行系统调用输出均出现，且第一项分别为 `1`、`2`、`3`，即可证明 3 个用户进程都被成功创建、调度并运行到了用户态。

如果在当前环境中不方便截取 QEMU 图形窗口，也可以用 GDB 在 `syscall_0` 入口处验证。实际调试中，连续 3 次断在 `syscall_0`，观察到参数如下：

```text
syscall_0(first=1, second=101, third=102, forth=103, fifth=104)
syscall_0(first=2, second=201, third=202, forth=203, fifth=204)
syscall_0(first=3, second=301, third=302, forth=303, fifth=304)
```

这说明 3 个用户进程分别执行到了自己的 `asm_system_call`，并通过 `int 0x80` 进入内核调用了同一个系统调用处理函数。

## 四、进程创建过程分析

### 1. 第一步：在线程 PCB 基础上扩展进程 PCB

进程创建入口是 `ProgramManager::executeProcess`：

```cpp
int ProgramManager::executeProcess(const char *filename, int priority)
{
    int pid = executeThread((ThreadFunction)load_process,
                            (void *)filename, filename, priority);
    ...
    PCB *process = ListItem2PCB(allPrograms.back(), tagInAllList);

    process->pageDirectoryAddress = createProcessPageDirectory();
    ...
    bool res = createUserVirtualPool(process);
    ...
    return pid;
}
```

可以看到，进程首先通过 `executeThread` 创建出一个普通 PCB。这个 PCB 已经包含线程调度所需的字段，例如 `stack`、`status`、`priority`、`pid` 和链表结点。

随后，`executeProcess` 继续为这个 PCB 添加进程独有资源：

```cpp
process->pageDirectoryAddress = createProcessPageDirectory();
createUserVirtualPool(process);
```

其中：

- `pageDirectoryAddress` 保存用户进程页目录表的虚拟地址。
- `userVirtual` 是用户进程自己的虚拟地址池。

因此，本实验中的进程 PCB 是在线程 PCB 的基础上扩展出来的。线程只需要内核调度上下文，进程还需要独立的用户虚拟地址空间。

### 2. 第二步：设置 ProcessStartStack 并通过 iret 进入用户态

用户进程第一次被调度时，并不是直接跳到用户进程函数，而是先执行 `load_process`：

```cpp
int pid = executeThread((ThreadFunction)load_process,
                        (void *)filename, filename, priority);
```

`load_process` 会在当前进程 PCB 顶部构造 `ProcessStartStack`：

```cpp
ProcessStartStack *interruptStack =
    (ProcessStartStack *)((int)process + PAGE_SIZE - sizeof(ProcessStartStack));
```

然后填入用户态运行所需的寄存器现场：

```cpp
interruptStack->ds = programManager.USER_DATA_SELECTOR;
interruptStack->es = programManager.USER_DATA_SELECTOR;
interruptStack->fs = programManager.USER_DATA_SELECTOR;
interruptStack->eip = (int)filename;
interruptStack->cs = programManager.USER_CODE_SELECTOR;
interruptStack->eflags = (0 << 12) | (1 << 9) | (1 << 1);
interruptStack->esp = memoryManager.allocatePages(AddressPoolType::USER, 1);
interruptStack->esp += PAGE_SIZE;
interruptStack->ss = programManager.USER_STACK_SELECTOR;
```

关键字段含义如下：

- `eip`：用户进程入口地址，即 `process_a/process_b/process_c`。
- `cs`：用户态代码段选择子，RPL=3。
- `ds/es/fs/ss`：用户态数据段和栈段选择子，RPL=3。
- `eflags`：打开中断，且 IOPL=0。
- `esp`：用户栈顶地址，由用户地址池分配一页得到。

最后调用：

```cpp
asm_start_process((int)interruptStack);
```

`asm_start_process` 的汇编逻辑是：

```asm
asm_start_process:
    mov eax, dword[esp+4]
    mov esp, eax
    popad
    pop gs
    pop fs
    pop es
    pop ds
    iret
```

它先把 `esp` 切到 `ProcessStartStack`，再依次弹出通用寄存器和段寄存器，最后执行 `iret`。`iret` 会弹出 `eip/cs/eflags/esp/ss`。由于这里的 `cs` 和 `ss` 都是 RPL=3 的用户态选择子，所以 CPU 完成从 CPL=0 到 CPL=3 的切换，开始执行用户进程入口函数。

### 3. 第三步：调度时切换页目录表

调度函数 `ProgramManager::schedule` 选出下一个 READY 程序后，会调用：

```cpp
activateProgramPage(next);
asm_switch_thread(cur, next);
```

`activateProgramPage` 的代码如下：

```cpp
void ProgramManager::activateProgramPage(PCB *program)
{
    int paddr = PAGE_DIRECTORY;

    if (program->pageDirectoryAddress)
    {
        tss.esp0 = (int)program + PAGE_SIZE;
        paddr = memoryManager.vaddr2paddr(program->pageDirectoryAddress);
    }

    asm_update_cr3(paddr);
}
```

如果被调度对象是用户进程，那么 `pageDirectoryAddress != 0`。内核会先把 `tss.esp0` 设置为当前进程 PCB 页的顶部，作为该进程从用户态进入内核态时使用的 0 特权级栈；然后将进程页目录表虚拟地址转换成物理地址，写入 `CR3`。

写入 `CR3` 后，CPU 使用新的页目录表完成地址转换。这样，每个用户进程都有自己的用户虚拟地址空间。同时，页目录表高 1GB 的内核部分是从内核页目录复制而来，所以切换到用户进程页目录后，内核代码和内核数据仍然可以正常访问。

## 五、pageDirectoryAddress 的区别

线程 PCB 中 `pageDirectoryAddress` 为 0，是因为内核线程只运行在内核态，共享内核页目录表，不需要独立的用户虚拟地址空间。

用户进程的 `pageDirectoryAddress` 不为 0，是因为 `executeProcess` 会调用：

```cpp
process->pageDirectoryAddress = createProcessPageDirectory();
```

这为用户进程创建了独立页目录表。调度时，内核通过判断 `pageDirectoryAddress` 是否为 0 来区分线程和进程：

```cpp
if (program->pageDirectoryAddress)
{
    tss.esp0 = (int)program + PAGE_SIZE;
    paddr = memoryManager.vaddr2paddr(program->pageDirectoryAddress);
}
```

因此：

```text
内核线程：pageDirectoryAddress = 0，使用内核页目录。
用户进程：pageDirectoryAddress != 0，调度时切换到自己的页目录。
```

## 六、实验结论

本实验创建了 3 个用户进程，并通过系统调用输出不同参数验证了它们都能被调度运行。进程创建的核心过程可以概括为：

1. 先用 `executeThread` 创建基础 PCB。
2. 再为 PCB 添加页目录表和用户虚拟地址池。
3. 首次运行时用 `ProcessStartStack` 伪造中断返回现场。
4. 通过 `asm_start_process` 的 `iret` 从内核态进入用户态。
5. 后续调度时通过切换 `CR3` 切换进程虚拟地址空间。

线程和进程的主要区别体现在 `pageDirectoryAddress`：线程没有独立地址空间，所以该字段为 0；进程拥有独立页目录表，所以该字段非 0。
