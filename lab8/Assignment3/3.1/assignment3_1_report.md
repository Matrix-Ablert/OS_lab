# Assignment 3.1 Exit 的实现与分析

## 一、实验目标

本实验基于 `Assignment3/3.1`，也就是课程材料中的 `src/5`，复现 `exit` 系统调用。实验创建一个父进程，父进程调用 `fork()` 创建子进程，子进程显式调用 `exit(42)` 退出。报告重点分析 `exit` 如何释放进程资源、如何将进程状态从 `RUNNING` 改为 `DEAD`、为什么退出后不会再被调度，以及进程函数正常返回时为什么会隐式调用 `exit`。

## 二、实验实现

测试进程定义在 `src/kernel/setup.cpp` 中：

```cpp
void first_process()
{
    int pid = fork();

    if (pid == -1)
    {
        printf("can not fork\n");
        asm_halt();
    }
    else
    {
        if (pid)
        {
            printf("I am parent, pid: %d, child pid: %d\n",
                   programManager.running->pid, pid);
            asm_halt();
        }
        else
        {
            printf("I am child, pid: %d, exit(42)\n",
                   programManager.running->pid);
            exit(42);
        }
    }
}
```

`first_thread` 只创建这个用户进程，避免其他线程输出干扰截图：

```cpp
void first_thread(void *arg)
{
    printf("start process\n");
    programManager.executeProcess((const char *)first_process, 1);
    asm_halt();
}
```

`setup_kernel` 注册了本实验需要的系统调用：

```cpp
systemService.setSystemCall(1, (int)syscall_write);
systemService.setSystemCall(2, (int)syscall_fork);
systemService.setSystemCall(3, (int)syscall_exit);
```

其中 `printf` 最终通过 `write()` 进入 1 号系统调用输出；`fork()` 使用 2 号系统调用；`exit(42)` 使用 3 号系统调用。

## 三、运行方法和预期结果

编译：

```bash
cd Assignment3/3.1/build
make clean && make build
```

运行：

```bash
make run
```

预期输出：

```text
start process
I am parent, pid: 1, child pid: 2
I am child, pid: 2, exit(42)
```

父子进程输出顺序可能受到调度影响，不要求完全固定。只要子进程打印 `exit(42)` 并进入 `ProgramManager::exit(42)`，即可证明显式退出路径正确。

本实验还没有实现 `wait`，因此父进程不会回收子进程 PCB。子进程退出后会进入 `DEAD` 状态，等待后续 3.2 的 wait 机制回收。

## 四、exit 执行流程

用户态调用：

```cpp
exit(42);
```

`src/kernel/syscall.cpp` 中的用户态封装为：

```cpp
void exit(int ret)
{
    asm_system_call(3, ret);
}

void syscall_exit(int ret)
{
    programManager.exit(ret);
}
```

完整路径如下：

```text
用户进程 exit(42)
-> asm_system_call(3, 42)
-> int 0x80
-> asm_system_call_handler
-> system_call_table[3]
-> syscall_exit(42)
-> ProgramManager::exit(42)
```

`ProgramManager::exit` 首先关闭中断，保存返回值并修改状态：

```cpp
PCB *program = this->running;
program->retValue = ret;
program->status = ProgramStatus::DEAD;
```

因此，子进程调用 `exit(42)` 后：

```text
running->retValue = 42
running->status   = DEAD
```

然后，如果当前退出对象是用户进程，即 `pageDirectoryAddress != 0`，内核会释放它占用的用户地址空间资源。

## 五、资源释放过程

`ProgramManager::exit` 遍历用户空间对应的前 768 个页目录项：

```cpp
for (int i = 0; i < 768; ++i)
{
    if (!(pageDir[i] & 0x1))
    {
        continue;
    }

    page = (int *)(0xffc00000 + (i << 12));
    ...
}
```

对每个存在的页表，继续遍历页表中的 1024 个页表项：

```cpp
for (int j = 0; j < 1024; ++j)
{
    if (!(page[j] & 0x1))
    {
        continue;
    }

    paddr = memoryManager.vaddr2paddr((i << 22) + (j << 12));
    memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
}
```

这一步释放用户进程拥有的用户物理页。

释放完用户物理页后，继续释放页表物理页：

```cpp
paddr = memoryManager.vaddr2paddr((int)page);
memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
```

最后释放页目录表和用户虚拟地址池 bitmap：

```cpp
memoryManager.releasePages(AddressPoolType::KERNEL, (int)pageDir, 1);

int bitmapBytes = ceil(program->userVirtual.resources.length, 8);
int bitmapPages = ceil(bitmapBytes, PAGE_SIZE);
memoryManager.releasePages(AddressPoolType::KERNEL,
                           (int)program->userVirtual.resources.bitmap,
                           bitmapPages);
```

因此，`exit` 会释放：

```text
用户物理页
用户页表页
用户页目录表页
用户虚拟地址池 bitmap
```

PCB 本身不会在 3.1 中立即回收，因为后续 `wait` 需要通过 PCB 读取子进程退出状态。

## 六、为什么 exit 后不会再被调度

`exit` 最后调用：

```cpp
schedule();
```

调度器只会从 `readyPrograms` 中选择下一个就绪程序：

```cpp
ListItem *item = readyPrograms.front();
PCB *next = ListItem2PCB(item, tagInGeneralList);
next->status = ProgramStatus::RUNNING;
running = next;
readyPrograms.pop_front();
```

而当前退出进程在 `exit` 中已经被设置为：

```cpp
program->status = ProgramStatus::DEAD;
```

它不会再被放回 `readyPrograms`。因此，`schedule()` 之后，CPU 会切换到其他 READY 程序，已经 `DEAD` 的进程不会再被调度执行。

需要注意：当前 3.1 还没有 `wait`，所以 DEAD 子进程的 PCB 可能暂时保留在 `allPrograms` 中。这是为了后续由父进程通过 `wait` 读取退出状态并回收 PCB。

## 七、正常返回为什么会隐式调用 exit

在早期进程实验中，用户进程函数正常返回可能会导致 CPU 跳到未知位置。3.1 中已经通过 `load_process` 在用户栈顶部预置了返回地址：

```cpp
interruptStack->esp = memoryManager.allocatePages(AddressPoolType::USER, 1);
interruptStack->esp += PAGE_SIZE;

int *userStack = (int *)interruptStack->esp;
userStack -= 3;
userStack[0] = (int)exit;
userStack[1] = 0;
userStack[2] = 0;

interruptStack->esp = (int)userStack;
```

用户进程启动时，`interruptStack->esp` 会成为用户态栈指针。进程函数执行完毕并执行 `ret` 时，会从当前用户栈顶弹出返回地址。这个返回地址正是 `userStack[0]` 中保存的 `exit`。

因此，用户进程函数正常返回时，会跳转到：

```text
exit(0)
```

其中返回值参数来自预置的栈内容。也就是说，显式写：

```cpp
return;
```

最终等价于隐式调用：

```cpp
exit(0);
```

## 八、GDB 验证建议

建议设置断点：

```gdb
b syscall_exit
b 'ProgramManager::exit(int)'
b 'ProgramManager::schedule()'
```

重点观察：

```gdb
p ret
p programManager.running->pid
p programManager.running->status
p programManager.running->retValue
n
p programManager.running->status
p programManager.running->retValue
```

预期在 `ProgramManager::exit(42)` 中看到：

```text
ret = 42
running->status 从 RUNNING 变为 DEAD
running->retValue = 42
```

本次实际 GDB 记录如下：

```text
ProgramManager::exit entry:
ret = 42
running = 0xc00260a0
running->pid = 2
running->status = RUNNING
running->retValue = 0

after retValue/status assignment:
ret = 42
running->pid = 2
running->status = DEAD
running->retValue = 42
running->pageDirectoryAddress = 0xc0118000
```

这说明子进程确实进入了 `exit(42)`，并且在释放进程资源之前已经保存返回值、将进程状态标记为 `DEAD`。

也可以在释放资源相关语句处观察 `pageDirectoryAddress`、用户页目录项和 `userVirtual.resources.bitmap`，确认退出的是用户进程时会释放对应资源。

## 九、实验结论

本实验验证了 `exit(42)` 的执行过程。子进程通过系统调用进入内核，内核保存返回值、将进程状态改为 `DEAD`，释放用户地址空间资源，并调用 `schedule()` 切换到其他程序。由于退出进程不会重新进入 READY 队列，所以它不会再被调度执行。

同时，`load_process` 在用户栈上预置 `exit` 作为进程函数的返回地址，因此进程函数即使正常返回，也会隐式执行 `exit(0)`，避免返回到未知地址。
