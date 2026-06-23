# Assignment 4.1 新增系统调用：getpid 和 sleep

## 一、实验目标

Assignment4 三个选做实验中，本实验选择 4.1。相比调度策略改进和 Shell 增强，`getpid/sleep` 的改动面更小：只需要在已有 `src/6` 进程管理基础上新增系统调用，并让时钟中断负责唤醒睡眠进程。

本实验基于 `Assignment3/3.2` 创建 `Assignment4/4.1`，实现：

- `int getpid()`：返回当前运行进程的 pid。
- `void sleep(int ticks)`：让当前进程阻塞指定时钟中断次数，到期后自动回到 ready 队列。

## 二、核心实现

### 1. getpid 系统调用

用户态封装位于 `src/kernel/syscall.cpp`：

```cpp
int getpid() {
    return asm_system_call(5);
}
```

内核态处理函数直接读取当前运行 PCB：

```cpp
int syscall_getpid() {
    return programManager.running->pid;
}
```

在 `setup.cpp` 中注册为 5 号系统调用：

```cpp
systemService.setSystemCall(5, (int)syscall_getpid);
```

### 2. sleep 系统调用

在 PCB 中增加 `sleepTicks` 字段，用来记录剩余睡眠时钟中断次数：

```cpp
int sleepTicks;
```

在 `ProgramManager` 中增加睡眠队列：

```cpp
List sleepingPrograms;
```

用户态 `sleep(ticks)` 通过 6 号系统调用进入内核：

```cpp
void sleep(int ticks) {
    asm_system_call(6, ticks);
}
```

内核态最终调用：

```cpp
void syscall_sleep(int ticks) {
    programManager.sleep(ticks);
}
```

`ProgramManager::sleep(int ticks)` 的逻辑是：

```cpp
running->sleepTicks = ticks;
running->status = ProgramStatus::BLOCKED;
sleepingPrograms.push_back(&(running->tagInGeneralList));
schedule();
```

当前进程正在 CPU 上运行，并不在 ready 队列中，因此可以复用 `tagInGeneralList` 挂入 sleep 队列。进程状态改为 `BLOCKED` 后，`schedule()` 不会把它重新放回 ready 队列。

### 3. 时钟中断唤醒

每次时钟中断先调用：

```cpp
programManager.wakeupSleepingPrograms();
```

`wakeupSleepingPrograms()` 遍历 sleep 队列，对每个阻塞进程递减 `sleepTicks`。当 `sleepTicks <= 0` 时：

```cpp
sleepingPrograms.erase(item);
program->status = ProgramStatus::READY;
readyPrograms.push_back(&(program->tagInGeneralList));
```

遍历时先保存 `next`，因为到期进程会从 sleep 队列中删除，直接继续使用当前结点会导致链表遍历失效。

## 三、测试设计

测试入口创建 3 个用户进程：

```cpp
programManager.executeProcess((const char *)process_a, 1);
programManager.executeProcess((const char *)process_b, 1);
programManager.executeProcess((const char *)process_c, 1);
```

三个进程分别调用 `getpid()` 打印自己的 pid，然后调用不同 tick 数的 `sleep`：

```cpp
printf("process A start, pid: %d\n", getpid());
sleep(20);
printf("process A wake, pid: %d\n", getpid());
exit(0);
```

其中：

- process A: `sleep(20)`
- process B: `sleep(40)`
- process C: `sleep(60)`

因此预期 A 先唤醒，B 第二，C 最后。

## 四、运行方法和预期结果

编译：

```bash
cd Assignment4/4.1/build
make clean && make build
```

运行：

```bash
make run
```

预期输出类似：

```text
start getpid/sleep test
process A start, pid: 1
process B start, pid: 2
process C start, pid: 3
process A wake, pid: 1
process B wake, pid: 2
process C wake, pid: 3
```

进程启动输出可能受调度影响略有交错，但唤醒顺序应大体符合 `20 < 40 < 60`。

## 五、GDB 验证

本次构建命令执行成功：

```bash
make clean && make build
```

使用 GDB 设置断点：

```gdb
b syscall_getpid
b syscall_sleep
b 'ProgramManager::sleep(int)'
b ../src/kernel/program.cpp:175
```

实际记录如下：

```text
GETPID running_pid=1 ret_should=1
SYSCALL_SLEEP pid=1 ticks=20 status=1
PM_SLEEP entry pid=1 ticks=20 status=1
GETPID running_pid=2 ret_should=2
SYSCALL_SLEEP pid=2 ticks=40 status=1
PM_SLEEP entry pid=2 ticks=40 status=1
GETPID running_pid=3 ret_should=3
SYSCALL_SLEEP pid=3 ticks=60 status=1
PM_SLEEP entry pid=3 ticks=60 status=1
WAKE pid=1 sleepTicks=0 status=3
GETPID running_pid=1 ret_should=1
WAKE pid=2 sleepTicks=0 status=3
GETPID running_pid=2 ret_should=2
WAKE pid=3 sleepTicks=0 status=3
```

其中 `ProgramStatus::RUNNING == 1`，`ProgramStatus::BLOCKED == 3`。可以看到：

- `getpid()` 返回值与 `programManager.running->pid` 一致。
- 三个进程分别进入 `sleep(20)`、`sleep(40)`、`sleep(60)`。
- 进入 sleep 后进程状态变为 `BLOCKED`。
- sleepTicks 递减到 0 后，进程按 A、B、C 顺序被唤醒。

## 六、实验结论

本实验通过新增 5 号和 6 号系统调用，实现了用户态获取 pid 和阻塞式 sleep。`sleep` 的关键不是忙等，而是把当前进程从运行态转为阻塞态，并依靠时钟中断在指定 ticks 后重新放回 ready 队列。这样既能验证系统调用机制，也能展示进程间基于 sleep 的简单同步。
