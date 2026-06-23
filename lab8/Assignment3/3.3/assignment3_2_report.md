# Assignment 3.2 Wait 与父子进程同步

## 一、实验目标

本实验基于 `Assignment3/3.2`，也就是课程材料中的 `src/6`，复现 `wait` 系统调用。实验要求父进程创建至少 2 个子进程，每个子进程以不同返回值调用 `exit`，父进程循环调用 `wait(&retval)` 收集所有子进程退出状态，并验证 `wait` 返回的 pid 和退出值是否正确。

## 二、实验实现

`Assignment3/3.2` 已经包含 `wait` 的核心实现。本实验主要修改 `src/kernel/setup.cpp` 的测试入口，让父进程创建两个直接子进程：

```cpp
void first_process()
{
    int retval;
    int child1 = fork();

    if (child1 == 0)
    {
        delay_for_child();
        printf("child pid: %d, exit(42)\n", programManager.running->pid);
        exit(42);
    }

    printf("parent pid: %d, child1 pid: %d\n", programManager.running->pid, child1);

    int child2 = fork();

    if (child2 == 0)
    {
        delay_for_child();
        printf("child pid: %d, exit(84)\n", programManager.running->pid);
        exit(84);
    }

    printf("parent pid: %d, child2 pid: %d\n", programManager.running->pid, child2);

    int pid;
    while ((pid = wait(&retval)) != -1)
    {
        printf("wait child pid: %d, retval: %d\n", pid, retval);
    }

    printf("all child processes collected, programs: %d\n",
           programManager.allPrograms.size());
    asm_halt();
}
```

`first_thread` 只创建这个父进程，避免额外线程输出干扰截图：

```cpp
void first_thread(void *arg)
{
    printf("start process\n");
    programManager.executeProcess((const char *)first_process, 1);
    asm_halt();
}
```

`setup_kernel` 保留 4 号系统调用注册：

```cpp
systemService.setSystemCall(4, (int)syscall_wait);
```

因此用户态 `wait(&retval)` 会通过 `asm_system_call(4, &retval)` 进入内核。

## 三、运行方法和预期结果

编译：

```bash
cd Assignment3/3.2/build
make clean && make build
```

运行：

```bash
make run
```

预期输出类似：

```text
start process
parent pid: 1, child1 pid: 2
parent pid: 1, child2 pid: 3
child pid: 2, exit(42)
child pid: 3, exit(84)
wait child pid: 2, retval: 42
wait child pid: 3, retval: 84
all child processes collected, programs: 2
```

父进程回收子进程的顺序由调度决定，不要求固定。如果先回收 pid 3，则对应退出值应为 84；如果先回收 pid 2，则对应退出值应为 42。

## 四、wait 的执行流程

用户态封装在 `src/kernel/syscall.cpp`：

```cpp
int wait(int *retval)
{
    return asm_system_call(4, (int)retval);
}

int syscall_wait(int *retval)
{
    return programManager.wait(retval);
}
```

完整调用路径为：

```text
用户进程 wait(&retval)
-> asm_system_call(4, &retval)
-> int 0x80
-> asm_system_call_handler
-> system_call_table[4]
-> syscall_wait(&retval)
-> ProgramManager::wait(&retval)
```

`ProgramManager::wait` 在一个循环中执行。每轮先关闭中断并遍历 `allPrograms`：

```cpp
item = this->allPrograms.head.next;
flag = true;
while (item)
{
    child = ListItem2PCB(item, tagInAllList);
    if (child->parentPid == this->running->pid)
    {
        flag = false;
        if (child->status == ProgramStatus::DEAD)
        {
            break;
        }
    }
    item = item->next;
}
```

这里通过 `child->parentPid == running->pid` 判断某个 PCB 是否为当前父进程的子进程。

## 五、找到 DEAD 子进程后的处理

如果 `item != nullptr`，说明找到了一个属于当前父进程且状态为 `DEAD` 的子进程：

```cpp
if (retval)
{
    *retval = child->retValue;
}

int pid = child->pid;
releasePCB(child);
interruptManager.setInterruptStatus(interrupt);
return pid;
```

处理步骤是：

1. 如果 `retval` 非空，将子进程退出值写入父进程提供的地址。
2. 保存子进程 pid，作为 `wait` 返回值。
3. 调用 `releasePCB(child)` 回收子进程 PCB。
4. 恢复中断状态并返回 pid。

`releasePCB` 会释放 PCB slot，并从 `allPrograms` 链表删除该 PCB：

```cpp
void ProgramManager::releasePCB(PCB *program)
{
    int index = ((int)program - (int)PCB_SET) / PCB_SIZE;
    PCB_SET_STATUS[index] = false;
    this->allPrograms.erase(&(program->tagInAllList));
}
```

## 六、没有 DEAD 子进程时的处理

如果没有找到 DEAD 子进程，则分两种情况：

```cpp
if (flag)
{
    interruptManager.setInterruptStatus(interrupt);
    return -1;
}
else
{
    interruptManager.setInterruptStatus(interrupt);
    schedule();
}
```

含义如下：

- `flag == true`：遍历中没有发现任何子进程，说明所有子进程都已经被回收，`wait` 返回 `-1`。
- `flag == false`：存在子进程，但它们还没有退出，父进程调用 `schedule()` 让出 CPU，等子进程运行并 `exit` 后再继续循环检查。

因此，父进程中的：

```cpp
while ((pid = wait(&retval)) != -1)
```

会持续回收所有子进程，直到没有子进程可回收为止。

## 七、exit 与 wait 的分工

`exit(ret)` 做的事情是：

1. 保存 `retValue = ret`。
2. 将进程状态设为 `DEAD`。
3. 释放用户页、页表、页目录和用户虚拟地址池 bitmap。
4. 调用 `schedule()`。

但是 `exit` 不立即释放 PCB。PCB 会保留 `pid`、`parentPid`、`retValue` 和 `status=DEAD`，供父进程后续 `wait` 使用。

`wait` 做的事情是：

1. 找到属于当前父进程的 `DEAD` 子进程。
2. 取出 `retValue`。
3. 返回子进程 pid。
4. 回收子进程 PCB。

所以 3.1 的 `exit` 和 3.2 的 `wait` 正好配合完成完整的子进程生命周期管理。

## 八、GDB 验证建议

建议设置断点：

```gdb
b 'ProgramManager::wait(int*)'
b 'ProgramManager::exit(int)'
b 'ProgramManager::releasePCB(PCB*)'
```

重点观察：

```gdb
p programManager.running->pid
p ret
p programManager.running->retValue
p programManager.running->status
```

在 `wait` 找到 DEAD 子进程后观察：

```gdb
p child->pid
p child->parentPid
p child->status
p child->retValue
p *retval
```

预期看到：

```text
child pid 2 -> retValue 42
child pid 3 -> retValue 84
wait 返回对应 child pid
父进程 retval 得到对应退出值
releasePCB 后 allPrograms 中子进程 PCB 被删除
```


本次实际 GDB 记录如下：

```text
exit call 1:
ret = 42
running->pid = 2

exit call 2:
ret = 84
running->pid = 3

wait found child 1:
child->pid = 2
child->retValue = 42
child->status = DEAD

wait found child 2:
child->pid = 3
child->retValue = 84
child->status = DEAD
```

这说明两个子进程分别以 42 和 84 退出，父进程的 `wait` 能找到对应的 DEAD 子进程，并读取正确的 `retValue`。

## 九、实验结论

本实验验证了 `wait` 的核心语义：父进程可以等待并回收已经退出的子进程，获取其退出值。如果子进程还未退出，父进程通过 `schedule()` 让出 CPU；如果已经没有子进程，`wait` 返回 `-1`。通过两个子进程分别 `exit(42)` 和 `exit(84)`，可以验证 `wait` 返回的 pid 和退出值对应关系正确。
