# Assignment 2.2 Fork 的实现与分析

## 一、实验目标

本实验基于 `Assignment2/2.2`，也就是课程材料中的 `src/4`，复现 fork 的实现。实验要求编写一个用户进程调用 `fork()`，让父进程和子进程分别输出不同信息，并结合代码分析 fork 如何创建子进程、复制地址空间、设置子进程内核栈，以及保证父子进程得到不同的返回值。

## 二、实验实现

`Assignment2/2.2` 已经复现了 `src/4` 的 fork 实现，核心逻辑位于：

- `src/kernel/syscall.cpp`：提供用户态 `fork()` 封装和内核态 `syscall_fork()`。
- `src/kernel/program.cpp`：实现 `ProgramManager::fork()` 和 `ProgramManager::copyProcess()`。
- `src/utils/asm_utils.asm`：实现 `asm_switch_thread` 和 `asm_start_process`。

本实验只调整 `src/kernel/setup.cpp` 中的测试输出，使父子进程都打印自己的 pid 和 fork 返回值：

```cpp
void first_process()
{
    int pid = fork();

    if (pid == -1)
    {
        printf("can not fork\n");
    }
    else
    {
        if (pid)
        {
            printf("I am parent, pid: %d, fork return child pid: %d\n",
                   programManager.running->pid, pid);
        }
        else
        {
            printf("I am child, pid: %d, fork return: %d\n",
                   programManager.running->pid, pid);
        }
    }

    asm_halt();
}
```

`setup_kernel()` 中保留了 1 号和 2 号系统调用的注册：

```cpp
systemService.setSystemCall(1, (int)syscall_write);
systemService.setSystemCall(2, (int)syscall_fork);
```

用户进程中的 `printf` 最终会调用 `write()`，而 `write()` 通过 `asm_system_call(1, ...)` 进入内核态输出。`fork()` 则通过 `asm_system_call(2)` 进入内核态，最后调用 `ProgramManager::fork()`。

## 三、运行方法和预期结果

编译：

```bash
cd Assignment2/2.2/build
make clean && make build
```

运行：

```bash
make run
```

预期输出类似：

```text
start process
I am parent, pid: 1, fork return child pid: 2
I am child, pid: 2, fork return: 0
```

父子进程的输出顺序由调度器决定，不要求固定。只要能看到父进程的 fork 返回值是子进程 pid，子进程的 fork 返回值是 0，即可说明 fork 行为正确。

## 四、fork 的四个关键问题

### 1. 如何为子进程创建独立的虚拟地址空间

`fork()` 的用户态封装在 `src/kernel/syscall.cpp`：

```cpp
int fork()
{
    return asm_system_call(2);
}

int syscall_fork()
{
    return programManager.fork();
}
```

真正创建子进程的是 `ProgramManager::fork()`：

```cpp
int ProgramManager::fork()
{
    PCB *parent = this->running;
    if (!parent->pageDirectoryAddress)
    {
        return -1;
    }

    int pid = executeProcess("", 0);
    PCB *child = ListItem2PCB(this->allPrograms.back(), tagInAllList);
    bool flag = copyProcess(parent, child);
    ...
    return pid;
}
```

这里先检查当前运行对象是否是用户进程。内核线程没有独立页目录表，`pageDirectoryAddress == 0`，因此不能调用 fork。

随后调用 `executeProcess("", 0)` 创建子进程 PCB。这个过程会为子进程分配新的页目录表和用户虚拟地址池。之后 `copyProcess(parent, child)` 会复制父进程的用户地址空间到子进程，使子进程拥有独立的页目录表、页表和物理页。

### 2. 如何复制父进程的用户空间数据到子进程

`copyProcess()` 会先获取父子进程页目录表：

```cpp
int childPageDirPaddr = memoryManager.vaddr2paddr(child->pageDirectoryAddress);
int parentPageDirPaddr = memoryManager.vaddr2paddr(parent->pageDirectoryAddress);
int *childPageDir = (int *)child->pageDirectoryAddress;
int *parentPageDir = (int *)parent->pageDirectoryAddress;
```

然后只处理用户空间对应的前 768 个页目录项：

```cpp
for (int i = 0; i < 768; ++i)
{
    if (!(parentPageDir[i] & 0x1))
    {
        continue;
    }
    ...
}
```

第一轮遍历为子进程分配页表，并复制页目录项的属性。第二轮遍历父进程页表，为每个存在的用户页分配新的用户物理页，再通过一页内核中转页复制内容：

```cpp
memcpy(pageVaddr, buffer, PAGE_SIZE);
asm_update_cr3(childPageDirPaddr);
pageTableVaddr[j] = (pte & 0x00000fff) | paddr;
memcpy(buffer, pageVaddr, PAGE_SIZE);
asm_update_cr3(parentPageDirPaddr);
```

需要注意：本实验里的 `memcpy` 定义是 `memcpy(src, dst, length)`，和 libc 的参数顺序不同。因此上面第一句是从父进程用户页复制到内核中转页，第二句是从中转页复制到子进程用户页。

### 3. 如何设置子进程的内核栈，使其第一次调度时能正确运行

当父进程调用 fork 后，会通过 `int 0x80` 进入内核态。CPU 已经把用户态返回现场保存到了父进程的 0 特权级栈中。`asm_system_call_handler` 进一步保存寄存器后，父进程 PCB 顶部的一段内容可以看作一个 `ProcessStartStack`。

`copyProcess()` 复制这段现场：

```cpp
ProcessStartStack *childpss =
    (ProcessStartStack *)((int)child + PAGE_SIZE - sizeof(ProcessStartStack));
ProcessStartStack *parentpss =
    (ProcessStartStack *)((int)parent + PAGE_SIZE - sizeof(ProcessStartStack));
memcpy(parentpss, childpss, sizeof(ProcessStartStack));
```

然后设置子进程第一次被 `asm_switch_thread` 调度时可恢复的栈：

```cpp
child->stack = (int *)childpss - 7;
child->stack[0] = 0;
child->stack[1] = 0;
child->stack[2] = 0;
child->stack[3] = 0;
child->stack[4] = (int)asm_start_process;
child->stack[5] = 0;
child->stack[6] = (int)childpss;
```

`asm_switch_thread` 切换到子进程栈后，会依次弹出 `esi/edi/ebx/ebp`，然后执行 `ret`。由于 `child->stack[4]` 是 `asm_start_process`，这个 `ret` 会跳到 `asm_start_process`。随后 `asm_start_process(childpss)` 把 `esp` 切到 `childpss`，恢复寄存器并执行 `iret`，从而让子进程沿着父进程 fork 后的返回路径继续执行。

### 4. 如何保证父进程返回子进程 pid，子进程返回 0

父进程的返回值来自 `ProgramManager::fork()`：

```cpp
return pid;
```

这里的 `pid` 是 `executeProcess("", 0)` 创建出的子进程 pid，所以父进程从系统调用返回后得到的是子进程 pid。

子进程不直接执行 `ProgramManager::fork()` 的返回语句。它第一次被调度时，会从复制来的 `ProcessStartStack` 恢复现场。`copyProcess()` 显式设置：

```cpp
childpss->eax = 0;
```

系统调用返回值通过 `eax` 传递，因此子进程从 `fork()` 返回时得到 0。

## 五、GDB 跟踪计划和记录点

建议设置断点：

```gdb
b 'ProgramManager::fork()'
b 'ProgramManager::copyProcess(PCB*, PCB*)'
b 'ProgramManager::schedule()'
b asm_switch_thread
b asm_start_process
```

在 `copyProcess()` 中重点观察：

```gdb
p/x parent
p/x child
p/x childpss
p/x childpss->eax
p/x child->stack
x/8wx child->stack
```

期望看到：

```text
childpss->eax = 0
child->stack[0..3] = 0
child->stack[4] = asm_start_process
child->stack[5] = 0
child->stack[6] = childpss
```

本次实际调试记录如下：

```text
parent   = 0xc0024e20
child    = 0xc0025e20
childpss = 0xc0026ddc
childpss->eax = 0x0
child->stack  = 0xc0026dc0
```

子进程栈内容：

```text
0xc0026dc0: 0x00000000  0x00000000  0x00000000  0x00000000
0xc0026dd0: 0xc0022c20  0x00000000  0xc0026ddc  0x00000000
```

其中 `0xc0022c20` 是 `asm_start_process`，`0xc0026ddc` 是 `childpss`。这正好对应 `asm_switch_thread` 恢复子进程栈后通过 `ret` 跳到 `asm_start_process(childpss)`。

在 `asm_switch_thread` 中跟踪子进程第一次被调度：

```gdb
info registers esp eip
si
info registers esp eip
x/8wx $esp
```

重点确认 `mov esp, [eax]` 后，`esp` 切换为子进程的 `child->stack`。随后 `ret` 跳到 `asm_start_process`，`asm_start_process` 使用 `childpss` 恢复现场，最后通过 `iret` 回到用户态的 fork 返回点。

本次用条件断点只在 `next == child` 时停在 `asm_switch_thread`，观察到：

```text
asm_switch_thread entry:
esp = 0xc0024d24
eip = 0xc0022d04
stack args: cur = 0xc0023e20, next = 0xc0025e20

asm_start_process entry:
esp = 0xc0026dd4
eip = 0xc0022c20
stack argument = 0xc0026ddc
```

这说明子进程第一次被调度时确实从 `asm_switch_thread` 进入 `asm_start_process`，参数就是前面复制并修改过的 `childpss`。

父子返回路径对比：

```text
父进程：
fork() -> int 0x80 -> syscall_fork -> ProgramManager::fork
       -> asm_system_call_handler -> iret -> asm_system_call -> fork 返回子进程 pid

子进程：
第一次被调度 -> asm_switch_thread -> asm_start_process -> iret
          -> asm_system_call_handler 返回路径 -> asm_system_call -> fork 返回 0
```

## 六、实验结论

fork 的本质是复制父进程的执行上下文和用户地址空间，并为子进程构造一个看起来像刚从系统调用中断返回的内核栈。父进程直接从 `ProgramManager::fork()` 返回子进程 pid；子进程则通过复制出来的 `ProcessStartStack` 恢复执行，并因为 `eax` 被设置为 0，所以从同一个 `fork()` 调用点返回 0。

这样就实现了“一次调用，两次返回”的 fork 语义。
