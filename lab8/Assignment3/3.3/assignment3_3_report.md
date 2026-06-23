# Assignment 3.3 僵尸进程与孤儿进程处理（方案 B）

## 一、实验目标

本实验基于 `Assignment3/3.2`，在已有 `fork/exit/wait` 的基础上处理两个问题：

1. 父进程不调用 `wait` 时，已经退出的子进程 PCB 会长期保持 `DEAD`，形成僵尸进程。
2. 父进程先于子进程退出时，仍存活的子进程变成孤儿，后续退出后无人回收。

本实验采用方案 B：父进程退出时自动回收已经 `DEAD` 的子进程，并将仍存活的子进程托管给系统 reaper。

## 二、实现思路

本实验使用 `pid=0` 的初始内核线程作为系统 reaper 的标记：

```cpp
const int REAPER_PID = 0;
```

当父进程退出时，内核扫描所有 PCB：

- 如果某个子进程已经是 `DEAD`，说明它已经成为僵尸，并且父进程马上退出后不会再 wait 它，因此立即 `releasePCB(child)`。
- 如果某个子进程仍然存活，则把它的 `parentPid` 改为 `REAPER_PID`，表示它已经被托管。
- 后续这个托管子进程退出时，`schedule()` 发现它是 `DEAD` 且 `parentPid == REAPER_PID`，就直接回收 PCB。

这样既能回收父进程退出前已经存在的僵尸，也能处理父进程退出后才结束的孤儿。

## 三、核心代码修改

### 1. 父进程退出时处理子进程

在 `ProgramManager::exit` 中，当前进程保存返回值并标记为 `DEAD` 后，调用：

```cpp
adoptOrReleaseChildren(program);
```

辅助函数逻辑如下：

```cpp
void ProgramManager::adoptOrReleaseChildren(PCB *parent)
{
    ListItem *item = this->allPrograms.head.next;

    while (item)
    {
        ListItem *next = item->next;
        PCB *child = ListItem2PCB(item, tagInAllList);

        if (child->parentPid == parent->pid)
        {
            if (child->status == ProgramStatus::DEAD)
            {
                releasePCB(child);
            }
            else
            {
                child->parentPid = REAPER_PID;
            }
        }

        item = next;
    }
}
```

这里遍历时先保存 `next`，是因为 `releasePCB(child)` 会从 `allPrograms` 中删除当前结点。如果不先保存下一个结点，链表遍历会失效。

### 2. schedule 自动回收托管孤儿

原来的 `schedule()` 只会直接回收 DEAD 内核线程，用户进程需要父进程 wait 回收。本实验保留这个规则，但额外允许 reaper 托管的孤儿被自动回收：

```cpp
else if (running->status == ProgramStatus::DEAD)
{
    if (!running->pageDirectoryAddress || running->parentPid == REAPER_PID)
    {
        releasePCB(running);
    }
}
```

含义是：

- `!running->pageDirectoryAddress`：内核线程没有父进程 wait，仍然直接回收。
- `running->parentPid == REAPER_PID`：孤儿进程已经托管给 reaper，退出后直接回收。
- 其他普通 DEAD 子进程仍保留 PCB，等待真实父进程 `wait`，所以不破坏 3.2 的语义。

## 四、测试设计

测试入口位于 `src/kernel/setup.cpp`。

父进程创建两个子进程：

```cpp
int child1 = fork();
```

child1 立即退出：

```cpp
printf("child1 pid: %d, exit(11)\n", programManager.running->pid);
exit(11);
```

父进程不调用 wait，而是延迟一段时间，让 child1 先进入 `DEAD` 状态。这样 child1 就成为僵尸。

然后父进程创建 child2：

```cpp
int child2 = fork();
```

child2 延迟后退出：

```cpp
delay_ticks(0xffffff);
printf("child2 pid: %d, exit(22)\n", programManager.running->pid);
exit(22);
```

父进程创建 child2 后不 wait，直接退出：

```cpp
printf("parent pid: %d, exit without wait\n", programManager.running->pid);
exit(99);
```

此时应发生两件事：

1. 父进程退出时发现 child1 已经 `DEAD`，立即回收 child1 的 PCB。
2. 父进程退出时发现 child2 仍存活，将 child2 的 `parentPid` 改为 `REAPER_PID`。

最后 child2 退出后，由 `schedule()` 自动回收。

额外创建一个观察线程：

```cpp
void observer_thread(void *arg)
{
    delay_ticks(0x3ffffff);
    printf("observer: cleanup finished, programs: %d\n",
           programManager.allPrograms.size());
    asm_halt();
}
```

观察线程延迟后输出 `allPrograms.size()`，用于截图观察清理后的进程数量。

## 五、运行方法和预期结果

编译：

```bash
cd Assignment3/3.3/build
make clean && make build
```

运行：

```bash
make run
```

预期输出类似：

```text
start zombie/orphan test
child1 pid: <child1_pid>, exit(11)
parent pid: 1, child1 pid: <child1_pid>
parent pid: 1, child2 pid: <child2_pid>
parent pid: 1, exit without wait
child2 pid: <child2_pid>, exit(22)
observer: cleanup finished, programs: <small number>
```

实际输出顺序可能受调度影响，但应能看到：

- child1 退出。
- 父进程未 wait 就退出。
- child2 在父进程退出后退出。
- observer 最后打印清理完成。

## 六、GDB 验证建议

建议设置断点：

```gdb
b 'ProgramManager::adoptOrReleaseChildren(PCB*)'
b 'ProgramManager::releasePCB(PCB*)'
b 'ProgramManager::schedule()'
b 'ProgramManager::exit(int)'
```

重点观察：

```gdb
p parent->pid
p child->pid
p child->parentPid
p child->status
```

预期记录：

```text
父进程退出时：
child1 status == DEAD，被 releasePCB 回收
child2 status != DEAD，parentPid 被改为 0

child2 退出后：
child2 status == DEAD && parentPid == 0
schedule 调用 releasePCB(child2)
```

本次 GDB 实际记录如下：

```text
EXIT ret=11 running_pid=3 parent=1 status=1 pageDir=c0118000
EXIT ret=99 running_pid=1 parent=0 status=1 pageDir=c0100000
ADOPT_MATCH parent=1 child=3 child_parent=1 child_status=4 child_pageDir=c0118000
RELEASE pid=3 parent=1 status=4 pageDir=c0118000
ADOPT_MATCH parent=1 child=4 child_parent=1 child_status=2 child_pageDir=c0118000
ADOPT_LIVE before parent=1 child=4 child_parent=1 child_status=2
RELEASE pid=1 parent=0 status=4 pageDir=c0100000
EXIT ret=22 running_pid=4 parent=0 status=1 pageDir=c0118000
RELEASE pid=4 parent=0 status=4 pageDir=c0118000
```

其中 `ProgramStatus::RUNNING == 1`，`READY == 2`，`DEAD == 4`。可以看出：

- child1 `pid=3` 先执行 `exit(11)`，父进程退出扫描时它已经是 `DEAD`，随后被 `releasePCB`。
- child2 `pid=4` 在父进程退出扫描时仍是 `READY`，因此被托管给 `parentPid=0`。
- child2 后续执行 `exit(22)` 时，断点记录里已经是 `parent=0`，随后被调度器自动释放。

## 七、实验结论

方案 B 的关键点是让父进程在退出时对自己的子进程负责：已经退出的子进程立即回收，仍存活的子进程转交给系统 reaper。这样既避免了父进程不 wait 造成的永久僵尸，也避免了孤儿进程退出后无人回收。

普通父进程仍然可以使用 `wait` 回收自己的 DEAD 子进程；只有父进程退出后留下的子进程才会被托管或自动回收，因此不会破坏 3.2 的 wait 语义。
