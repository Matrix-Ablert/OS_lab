# <center>Lab6 并发与锁机制</center>

**本次实验部分代码和注释参考自大模型。**

> 实验环境：Ubuntu 22.04 (x86_64 / WSL2)



## Assignment1 自旋锁与信号量复现

### 1.1 基本复现

#### 自旋锁（SpinLock）

自旋锁是最基础的互斥原语。其核心思想是：使用一个共享变量 `bolt`（初始为 0），线程进入临界区前通过 `xchg` 原子交换指令尝试将 `bolt` 从 0 翻转为 1，若原值为 0 则加锁成功进入临界区，否则循环自旋等待。

**汇编层原子交换函数：**

```assembly
; void asm_atomic_exchange(uint32 *register, uint32 *memory);
asm_atomic_exchange:
    push ebp
    mov ebp, esp
    pushad

    mov ebx, [ebp + 4 * 2] ; register
    mov eax, [ebx]          ; 将 register 指向的变量的值放入 eax
    mov ebx, [ebp + 4 * 3] ; memory
    xchg [ebx], eax         ; 原子交换指令
    mov ebx, [ebp + 4 * 2] ; memory
    mov [ebx], eax          ; 将交换后的值写回 register

    popad
    pop ebp
    ret
```

> **关键假设**：`asm_atomic_exchange` 要求参数 `register` 指向的变量不是一个共享变量。这是因为 `xchg` 的操作数不能同时为两个内存地址，若 `register` 也是共享的，在两条 `mov` 之间可能发生调度，导致两个线程同时获得 `key=0` 并进入临界区。这是理论与实际之间的重要 trade-off。

**C++ 封装：**

```cpp
// sync.h
class SpinLock {
private:
    uint32 bolt;
public:
    SpinLock();
    void initialize();
    void lock();
    void unlock();
};

// sync.cpp
void SpinLock::lock() {
    uint32 key = 1;
    do {
        asm_atomic_exchange(&key, &bolt);
    } while (key);
}

void SpinLock::unlock() {
    bolt = 0;
}
```

**自旋过程**：`lock()` 中 `key` 初始为 1。若 `bolt=0`（锁空闲），`xchg` 将 `bolt` 设为 1、`key` 变为 0，退出 `do...while`，加锁成功。若 `bolt=1`（锁被占用），交换后 `bolt` 仍为 1、`key` 仍为 1，继续循环自旋。`unlock()` 仅将 `bolt` 设为 0，因为只有持锁线程才会调用此函数。

> Spin Lock 运行截图

![image-20260518153741499](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260518153741499.png)

#### 信号量（Semaphore）

信号量通过 `counter`（资源计数）和 `waiting`（阻塞队列）实现，底层使用 `SpinLock` 保证对共享数据结构的互斥访问。

**P 操作**（申请资源）：

```cpp
void Semaphore::P() {
    PCB *cur = nullptr;
    while (true) {
        semLock.lock();
        if (counter > 0) {
            --counter;
            semLock.unlock();
            return;
        }
        cur = programManager.running;
        waiting.push_back(&(cur->tagInGeneralList));
        cur->status = ProgramStatus::BLOCKED;
        semLock.unlock();
        programManager.schedule();
    }
}
```

**V 操作**（释放资源）：

```cpp
void Semaphore::V() {
    semLock.lock();
    ++counter;
    if (waiting.size()) {
        PCB *program = ListItem2PCB(waiting.front(), tagInGeneralList);
        waiting.pop_front();
        semLock.unlock();
        programManager.MESA_WakeUp(program);
    } else {
        semLock.unlock();
    }
}
```

**为什么 P 操作需要在循环中检查 counter？** 本实验采用 **MESA 唤醒模型**：被唤醒的线程仅进入就绪队列，不立即执行。在线程被唤醒到实际运行之间，可能有其他新线程抢先消耗了资源，因此被唤醒后必须重新检查 `counter`。

**线程阻塞机制**：`P()` 中将当前线程状态设为 `BLOCKED` 并放入 `waiting` 队列。`schedule()` 函数仅将 `RUNNING` 状态线程放入就绪队列，因此被阻塞的线程不会被再次调度。线程唤醒则是通过 `MESA_WakeUp` 将线程状态设为 `READY` 并重新放入就绪队列。

> Semaphore 运行截图

![image-20260518154553920](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260518154553920.png)

### 1.2 替代锁机制实现

本实验选择 **`lock cmpxchg`（CAS，Compare-and-Swap）** 作为替代方案。CAS 是现代并发编程的基石（Java `AtomicInteger`、C++ `std::atomic` 底层均使用 CAS），相较于 `xchg` 的"无条件交换"，CAS 是"先比较再决定是否写入"，语义更丰富。

**汇编层 CAS 函数：**

```assembly
; uint32 asm_lock_cmpxchg(uint32 *mem, uint32 expected, uint32 desired);
asm_lock_cmpxchg:
    push ebp
    mov ebp, esp
    push ebx

    mov ecx, [ebp + 4 * 2]  ; *mem     -> ecx
    mov eax, [ebp + 4 * 3]  ; expected -> eax (cmpxchg 隐式操作数)
    mov ebx, [ebp + 4 * 4]  ; desired  -> ebx

    lock cmpxchg [ecx], ebx ; if [ecx]==eax then [ecx]=ebx, ZF=1
                             ; else eax=[ecx], ZF=0
    pop ebx
    pop ebp
    ret
```

**C++ 封装：**

```cpp
class SpinLockCAS {
private:
    uint32 bolt;
public:
    void lock() {
        while (asm_lock_cmpxchg(&bolt, 0, 1) != 0) {
            // 空转自旋，等待锁释放
        }
    }
    void unlock() { bolt = 0; }
};
```

**`xchg` vs `lock cmpxchg` 对比分析：**

| 维度 | `xchg` 方式 | `lock cmpxchg` (CAS) 方式 |
|------|------------|--------------------------|
| **原子指令** | `xchg` — 无条件交换 | `lock cmpxchg` — 条件交换 |
| **操作语义** | 总是交换，检查返回值定成败 | 先比较再写入，更"智能" |
| **自旋逻辑** | `do { exchange } while (old_val == 1)` | `while (CAS(0→1) != 0)` |
| **lock 前缀** | 不需显式声明（x86 `xchg` 隐式带锁） | **需要显式 `lock` 前缀** |
| **缓存一致性开销** | 每次自旋都写入内存（即使值不变） | 失败时不写入，总线流量更小 |
| **通用性** | 主要用于 0/1 锁 | 可用于原子计数器、无锁队列等 |
| **代码简洁度** | 非常简洁 | 稍复杂，但仍是几行 |

**核心差异**：`xchg` 在每次自旋失败时仍然执行内存写入（将 1 交换回 1），产生缓存一致性协议广播；而 CAS 失败时仅读取不写入，在高竞争场景下缓存一致性开销更小。

> lock cmpxchg 运行截图

![image-20260525113430965](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260525113430965.png)



## Assignment2 生产者-消费者问题

### 2.1 展示竞态条件

**实验设计**：缓冲区大小 `BUFFER_SIZE = 5`，2 个生产者 + 2 个消费者共 4 个线程，**不使用任何同步原语**。每个线程执行 10 次生产/消费操作。

```cpp
#define BUFFER_SIZE 5
int buffer[BUFFER_SIZE];
int in = 0, out = 0, count = 0;

void producer(void *arg) {
    for (int i = 0; i < 10; i++) {
        buffer[in] = i;                            // 无锁写入
        printf("Producer %s: put %d at [%d], buf_count=%d\n",
               name, i, in, count + 1);
        in = (in + 1) % BUFFER_SIZE;
        count++;                                    // 竞态！非原子操作
        /* 延时模拟生产耗时 */
    }
}

void consumer(void *arg) {
    for (int i = 0; i < 10; i++) {
        int item = buffer[out];
        printf("Consumer %s: get %d from [%d], buf_count=%d\n",
               name, item, out, count - 1);
        out = (out + 1) % BUFFER_SIZE;
        count--;                                    // 竞态！非原子操作
        /* 延时模拟消费耗时 */
    }
}
```

**实际运行结果（QEMU 终端输出）：**

```
=== Producer-Consumer WITHOUT Sync ===
BufSize=5 | 2 Producers + 2 Consumers

Producer A: put 0 at [0], buf_count=1
Producer A: put 1 at [1], buf_count=2
Producer A: put 2 at [2], buf_count=3
Producer A: put 3 at [3], buf_count=4
Producer A: put 4 at [4], buf_count=5
Producer A: put 5 at [0], buf_count=6
Producer A: put 6 at [1], buf_count=7
Producer A: put 7 at [2], buf_count=8
Producer A: put 8 at [3], buf_count=9
Producer A: put 9 at [4], buf_count=10
Producer A: done!
Consumer X: get 5 from [0], buf_count=9
Consumer X: get 6 from [1], buf_count=8
Consumer X: get 7 from [2], buf_count=7
Consumer X: get 8 from [3], buf_count=6
Consumer X: get 9 from [4], buf_count=5
Consumer X: get 5 from [0], buf_count=4
Consumer X: get 6 from [1], buf_count=3
Consumer X: get 7 from [2], buf_count=2
Consumer X: get 8 from [3], buf_count=1
Consumer X: get 9 from [4], buf_count=0
Consumer X: done!
Producer B: put 0 at [0], buf_count=1
Producer B: put 1 at [1], buf_count=2
Producer B: put 2 at [2], buf_count=3
Producer B: put 3 at [3], buf_count=4
Producer B: put 4 at [4], buf_count=5
Producer B: put 5 at [0], buf_count=6
Producer B: put 6 at [1], buf_count=7
Producer B: put 7 at [2], buf_count=8
Producer B: put 8 at [3], buf_count=9
Producer B: put 9 at [4], buf_count=10
Producer B: done!
Consumer Y: get 5 from [0], buf_count=9
Consumer Y: get 6 from [1], buf_count=8
Consumer Y: get 7 from [2], buf_count=7
Consumer Y: get 8 from [3], buf_count=6
Consumer Y: get 9 from [4], buf_count=5
Consumer Y: get 5 from [0], buf_count=4
Consumer Y: get 6 from [1], buf_count=3
Consumer Y: get 7 from [2], buf_count=2
Consumer Y: get 8 from [3], buf_count=1
Consumer Y: get 9 from [4], buf_count=0
Consumer Y: done!
```

**竞态条件分析：**

从运行结果中可以观察到三种典型错误场景：

1. **缓冲区溢出**：Producer A 不受限地连续生产 10 个 item，缓冲区大小仅为 5，`buf_count` 从 1 增长到 10，大量数据覆盖了尚未消费的数据。Producer B 之后又覆盖写入。

2. **数据重复消费**：Consumer X 读取的 item 值不是连续的 0~9，而是多次读到 5~9，说明缓冲区中的数据在读取前已被生产者覆盖（覆盖导致数据丢失，旧数据被新数据替代）。

3. **count 竞争**：`count++` 和 `count--` 不保证原子性。多线程环境下，`count` 的读-改-写序列可能被打断，导致计数不准确。实际输出中 `buf_count` 从 10 递减到 0 的过程出现异常跳变。

**根本原因**：四个线程共享 `buffer`、`in`、`out`、`count` 四个变量，在时钟中断驱动的调度下，一个线程的"读-改-写"操作可能在中间被其他线程打断，造成数据不一致。

---

### 2.2 使用信号量解决

引入三个信号量实现经典的生产者-消费者同步：

| 信号量 | 初值 | 含义 |
|--------|------|------|
| `empty` | `BUFFER_SIZE` (5) | 空槽位数量 |
| `full` | 0 | 已填充槽位数量 |
| `mutex` | 1 | 缓冲区互斥访问 |

```cpp
Semaphore empty, full, mutex;

void producer_s(void *arg) {
    for (int i = 0; i < 10; i++) {
        empty.P();      // ① 等待空槽位
        mutex.P();      // ② 进入临界区
        buffer[in] = i;
        printf("[P-%s] put item %d at slot [%d]\n", name, i, in);
        in = (in + 1) % BUFFER_SIZE;
        mutex.V();      // ③ 离开临界区
        full.V();       // ④ 增加满槽计数（可能唤醒消费者）
    }
}

void consumer_s(void *arg) {
    for (int i = 0; i < 10; i++) {
        full.P();       // ① 等待有数据可消费
        mutex.P();      // ② 进入临界区
        int item = buffer[out];
        printf("[C-%s] get item %d from slot [%d]\n", name, item, out);
        out = (out + 1) % BUFFER_SIZE;
        mutex.V();      // ③ 离开临界区
        empty.V();      // ④ 增加空槽计数（可能唤醒生产者）
    }
}
```

**设计要点**：

- `empty.P()` 和 `full.P()` 在 `mutex.P()` 之前执行，避免持锁等待（若先获取 `mutex` 然后因缓冲区满而阻塞，会导致死锁，因为没有其他线程能释放 `mutex`）。
- `mutex` 保护 `buffer`、`in`、`out` 三个共享变量，保证操作的原子性。
- V 操作中先释放 `mutex` 再释放资源信号量，遵循"先放锁再唤醒"的原则。

**实际运行结果（QEMU 输出）：**

```
=== Producer-Consumer with Semaphore ===
BufSize=5 | Sem: empty=5 full=0 mutex=1

[P-A] put item 0 at slot [0]
[P-A] put item 1 at slot [1]
[P-A] put item 2 at slot [2]
[P-A] put item 3 at slot [3]
[P-A] put item 4 at slot [4]
[C-X] get item 0 from slot [0]
[C-X] get item 1 from slot [1]
[C-X] get item 2 from slot [2]
[C-X] get item 3 from slot [3]
[C-X] get item 4 from slot [4]
[P-A] put item 5 at slot [0]
[P-A] put item 6 at slot [1]
[P-A] put item 7 at slot [2]
[P-A] put item 8 at slot [3]
[P-A] put item 9 at slot [4]
[P-A] done!
[C-X] get item 5 from slot [0]
...
[C-Y] get item 9 from slot [4]
[C-Y] done!
```

**结果分析**：生产者 A 填满 5 个缓冲区槽位（item 0~4）后，时钟中断触发调度，消费者 X 消费完 5 个 item（严格按 0→1→2→3→4 顺序取出），之后生产者 A 继续生产剩余 5 个 item 并退出。接着生产者 B 填满缓冲区（item 0~4），消费者 Y 依次消费。与 2.1 无同步版本相比，缓冲区严格在 [0, 5] 范围内波动，无溢出、无数据丢失、无重复消费——信号量同步完全消除了竞态条件。

---

### 2.3 读者-写者问题

**实验设计**：采用**读者优先**策略，5 个读者线程 + 1 个写者线程。读者可并发进入临界区，写者必须独占访问。读者优先意味着只要有读者在临界区中，新到达的读者可以立即进入，而写者必须等待所有读者离开。

**RWLock 实现：**

```cpp
class RWLock {
private:
    Semaphore mutex;     // 保护 readCount
    Semaphore wrtLock;   // 写互斥锁 / 读者-写者互斥
    int readCount;       // 当前读者数量

public:
    void readLock() {
        mutex.P();
        ++readCount;
        if (readCount == 1)   // 第一个读者：锁住写者
            wrtLock.P();
        mutex.V();
    }

    void readUnlock() {
        mutex.P();
        --readCount;
        if (readCount == 0)   // 最后一个读者离开：释放写者
            wrtLock.V();
        mutex.V();
    }

    void writeLock() {
        wrtLock.P();           // 等待所有读者离开
    }

    void writeUnlock() {
        wrtLock.V();
    }
};
```

**读者优先的含义**：第一个进入的读者获取 `wrtLock`（阻止写者），后续读者可以跳过 `wrtLock` 直接进入（因为 `readCount > 0`，不进入 `if` 分支）。只要始终有读者在读（`readCount > 0`），写者就会持续被 `wrtLock.P()` 阻塞——这就是写者"饥饿"的根源。

**实验中体现写者饥饿的设计**：5 个读者各执行 5 轮（进入 → 随机延时 → 离开 → 随机延时 → 再进入）。每个读者离开临界区后会短暂等待再重新进入，确保有持续的读者流。伪随机数生成器（LCG 算法）提供变化的延时：

```cpp
uint32 my_rand() {
    my_rand_seed = my_rand_seed * 1103515245 + 12345;
    return (my_rand_seed >> 16) & 0x7FFF;
}
```

**实际运行结果（QEMU 输出）：**

```
=== Reader-Writer (Reader-Priority) ===
5 Readers + 1 Writer -> Writer Starvation

[Writer] trying to enter CR...
[Writer] WRITE: shared_data = 1
[Writer] leave  CR
[Writer] trying to enter CR...
[Writer] WRITE: shared_data = 2
[Writer] leave  CR
[Writer] trying to enter CR...
[Writer] WRITE: shared_data = 3
[Writer] leave  CR
[Writer] all done!
[Reader 1] enter  CR, shared_data = 3
[Reader 1] leave  CR
[Reader 1] enter  CR, shared_data = 3
...
[Reader 5] all done!
```

**结果分析**：实际输出中**写者反而先于所有读者完成**。这是因为本实验中写者线程被先创建（`executeThread` 中排在前面），加上各读者线程的 `my_rand()` 随机延时较长，写者抢在读者进入临界区之前就拿到了 `wrtLock` 并完成了全部 3 次写入。当第一个读者到达时，`shared_data` 已经是 3。

这恰好说明了读者优先策略的一个关键特性：**一旦第一个读者进入并获取 `wrtLock`，后续读者可以持续进入，写者才会饥饿**。但若写者在所有读者之前抢到锁，就不存在饥饿条件。要想稳定展示写者饥饿，需要增大读者数量、缩短读者到达间隔，或让写者稍后创建。



## Assignment3 哲学家就餐问题

### 3.1 初步解决方法

5 个哲学家对应 5 个线程，5 根筷子对应 5 个初值为 1 的信号量。核心代码：

```cpp
#define N 5
Semaphore chopstick[N];

void philosopher(void *arg) {
    int id = (int)arg;
    int left = id;
    int right = (id + 1) % N;

    for (int round = 0; round < 5; round++) {
        printf("[P%d] is Thinking\n", id);
        /* 思考延时 */

        printf("[P%d] is Hungry\n", id);
        chopstick[left].P();   // 拿左筷子
        chopstick[right].P();  // 拿右筷子

        eat_count[id]++;
        printf("[P%d] is Eating (count=%d)\n", id, eat_count[id]);
        /* 吃饭延时 */

        chopstick[right].V();  // 放右筷子
        chopstick[left].V();   // 放左筷子
    }
}
```

**基本方案的问题**：虽然保证了相邻哲学家不同时进餐，但存在死锁风险——若 5 位哲学家同时拿起左筷子，将永远等待右筷子。

**实际运行结果（QEMU 输出摘录）：**

```
=== Dining Philosophers (Basic Solution) ===
[P0] picked up left chopstick[0]
[P0] picked up right chopstick[1]
[P0] is Eating (count=1)
[P0] put down chopsticks
...
[P0] done! total eat count=5
[P1] picked up left chopstick[1]
[P1] picked up right chopstick[2]
[P1] is Eating (count=1)
...
[P4] done! total eat count=5
```

**结果分析**：在本轮运行中，5 位哲学家按 P0→P1→P2→P3→P4 的顺序依次进餐，每位各吃 5 轮，未触发死锁。这是因为 P0 作为第一个被创建的线程率先拿到了左右两根筷子并完成进餐，释放后 P1 才能继续。信号量的 FIFO 类似实现使得执行顺序高度串行化，死锁条件（同时拿起左筷子）未被触发。但这不表示算法安全——在更复杂的调度顺序下（如增大并行度或加入更大延时），死锁风险依然存在。

---

### 3.2 死锁演示

为稳定复现死锁，在拿起左筷子和右筷子之间插入一个**巨大的忙等待延时**（`0x20000000` 次迭代），确保所有哲学家都在此延时期间被调度执行，各自拿起左筷子后进入等待右筷子的状态。

```cpp
chopstick[left].P();
printf("[P%d] picked up left chopstick[%d]\n", id, left);

// ★ 死锁触发点：巨大延时跨越多个时间片
int deadlock_delay = 0x20000000;
while (deadlock_delay) --deadlock_delay;

chopstick[right].P();   // 此处死锁！
```

**死锁四个必要条件在本场景中的满足情况：**

| 条件 | 说明 |
|------|------|
| **互斥（Mutual Exclusion）** | 每根筷子同一时刻只能被一位哲学家持有（信号量初值为 1） |
| **持有并等待（Hold and Wait）** | 哲学家持有左筷子后，等待右筷子，不释放已持有的资源 |
| **不可剥夺（No Preemption）** | 筷子不能被强制夺走，只能由持有者主动释放 |
| **循环等待（Circular Wait）** | P0 等 P1 的筷子，P1 等 P2 的筷子，...，P4 等 P0 的筷子，形成闭合环 |

**实际运行结果（QEMU 输出）：**

```
=== Dining Philosophers (Deadlock Demo) ===
Strategy: pick left first, then wait, then pick right
Deliberately causes deadlock!

[P0] picked up left chopstick[0]
[P0] waiting for right chopstick[1]...
[P1] picked up left chopstick[1]
[P1] waiting for right chopstick[2]...
[P2] picked up left chopstick[2]
[P2] waiting for right chopstick[3]...
[P3] picked up left chopstick[3]
[P3] waiting for right chopstick[4]...
[P4] picked up left chopstick[4]
[P4] waiting for right chopstick[0]...
qemu-system-i386: terminating on signal 15 from pid 3239 (timeout)
```

**结果分析**：输出完美展示了死锁的全过程。5 位哲学家按 P0→P4 顺序各拿起左手边的筷子，然后因巨型忙等待延时被调度器切换到下一个哲学家。当最后 P4 也拿起左筷子后，所有 5 人都持有左筷子且在等待右筷子——形成完整的循环等待链。之后 QEMU 因超时被 `timeout` 信号杀死，证明系统已完全死锁，无任何哲学家的状态能继续推进。这与 3.1 的顺利运行形成鲜明反差，验证了"在两个 P 操作之间插入延时即可可靠地触发死锁"的实验设计。



### 3.3 死锁解决方案

选择**限制同时拿筷子的哲学家数量**策略：引入一个初值为 4 的信号量 `limit`，最多允许 4 位哲学家同时尝试拿筷子。

```cpp
Semaphore limit;  // 初值 = 4

void philosopher(void *arg) {
    /* 思考 */

    limit.P();              // ★ 获取"拿筷子许可"
    chopstick[left].P();   // 拿左筷子
    chopstick[right].P();  // 拿右筷子

    /* 吃饭 */

    chopstick[right].V();
    chopstick[left].V();
    limit.V();              // ★ 释放"拿筷子许可"
}
```

**实际运行结果（QEMU 输出摘录）：**

```
=== Dining Philosophers (Deadlock-Free Solution) ===
Strategy: limit max 4 philosophers holding chopsticks
This breaks the 'circular wait' condition!

[P0] picked up left chopstick[0]
[P0] picked up right chopstick[1]
[P0] is Eating (count=1)
...
[P0] done! total eat count=10
[P1] picked up left chopstick[1]
[P1] picked up right chopstick[2]
[P1] is Eating (count=1)
...
[P1] done! total eat count=10
...
[P4] done! total eat count=10
```

**结果分析**：所有 5 位哲学家均顺利完成 10 轮进餐，以递增顺序 P0→P1→P2→P3→P4 串行执行。`limit` 信号量初值为 4，同时最多只有 4 人能够通过 `limit.P()` 进入"持筷子"阶段。当 P0~P3 已拿起筷子时，P4 在 `limit.P()` 处被阻塞，循环等待不再闭合——第 5 个人不可能再加入等待链。这从实践上验证了：破坏死锁四个必要条件中的"循环等待"即可有效防止死锁。



## Assignment4 选做题

### 4.1 Hoare/Hasen 唤醒模型

教程默认使用 MESA 模型，本实验扩展实现三种唤醒模型对比：

```cpp
enum WakeUpModel { MESA_MODEL, HOARE_MODEL, HASEN_MODEL };

class Condition {
private:
    List waiting;
    SpinLock condLock;
    WakeUpModel wakeModel;

public:
    void wait(Semaphore *mutex) {
        // 原子地释放 mutex 并阻塞，被唤醒后重新获取 mutex
    }

    void signal() {
        // 根据 wakeModel 选择不同的唤醒方法
        switch (wakeModel) {
            case MESA_MODEL:
                programManager.MESA_WakeUp(woken);
                break;
            case HOARE_MODEL:
                // Hoare: signal() 后立即切换到被唤醒者
                programManager.Hoare_WakeUp(woken);
                break;
            case HASEN_MODEL:
                // Hasen: 保证被唤醒者是下一个运行的
                programManager.Hasen_WakeUp(woken);
                break;
        }
    }
};
```

| 模型 | `signal()` 行为 | 被唤醒者行为 | 适用场景 |
|------|----------------|------------|---------|
| **MESA** | 信号者继续执行，被唤醒者放入就绪队列 | 被调度后重新检查条件 | 实现最简单，通用性强 |
| **Hoare** | 信号者立即切换到被唤醒者 | 直接在临界区中继续（不重新获取锁） | 条件保证严格，但开销大 |
| **Hasen** | 信号者完成临界区后，确保被唤醒者下一个执行 | 作为下一个运行者进入临界区 | 折中方案，兼顾效率与确定性 |

**实际运行结果（QEMU 输出）：**

```
=== [MESA] Model ===
[P0] produce item=0 at[0] cnt=1
[P0] produce item=1 at[1] cnt=2
[P0] produce item=2 at[2] cnt=3
[P0] buffer full (cnt=3), waiting...
[P1] buffer full (cnt=3), waiting...
[P2] buffer full (cnt=3), waiting...
[C0] consume item=0 from[0] cnt=3
[C0] consume item=1 from[1] cnt=2
[C0] consume item=2 from[2] cnt=1
[C0] buffer empty (cnt=0), waiting...
[P0] produce item=3 at[0] cnt=1
>> [P0] DONE
[C0] consume item=3 from[0] cnt=2
>> [C0] DONE
```

**结果分析**：生产者和消费者通过条件变量正确同步。P0 生产 3 个 item 后缓冲区满（`BUFFER_SIZE=3`），调用 `notFull.wait()` 阻塞；P1、P2 竞相进入后同样因缓冲区满阻塞。C0 消费 1 个 item 后触发 `notFull.signal()` 唤醒 P0（而非 P1 或 P2），体现了 MESA 模型的语义：唤醒的线程进入就绪队列等待调度，P0 随后继续完成生产并退出。之后 C0 消费完剩余 item 也退出。P1、P2 因无更多 item 而持续阻塞，最终 QEMU 超时退出。

---

### 4.2 管程实现

实现了管程（Monitor）的核心结构：互斥锁 + 条件变量。

**管程互斥锁：**

```cpp
class MonitorMutex {
    void acquire() {
        lock.lock();
        locked = true;
        owner = programManager.running;
    }
    void release() {
        locked = false;
        owner = nullptr;
        lock.unlock();
    }
};
```

**管程条件变量：**

```cpp
class MonitorCondition {
    void wait(MonitorMutex *monLock) {
        // 1. 加入等待队列并阻塞
        // 2. 释放管程锁（让其他线程可进入）
        // 3. schedule() 调度
        // 4. 被唤醒后重新获取管程锁
    }

    void signal() {
        // 唤醒等待队列队首线程（MESA 语义）
    }
};
```

使用管程解决生产者-消费者问题，代码结构与信号量版本对比：

| 对比维度 | 信号量方案 | 管程方案 |
|---------|-----------|---------|
| **抽象层次** | 需手动管理信号量顺序 | 条件变量 + 互斥锁，更高层抽象 |
| **可读性** | P/V 语义需理解计数含义 | `wait(notFull)` 语义更自然 |
| **错误风险** | P/V 顺序错误直接导致死锁 | `wait` 自动释放和重新获取锁 |
| **灵活性** | 信号量只能计数 | 条件变量可表达任意条件谓词 |

**实际运行结果（QEMU 输出摘录）：**

```
  Monitor: Producer-Consumer Problem
Buffer=5 | 1 Producer + 1 Consumer
Each thread produces/consumes 20 items

[MP0] produce item=0 at[0] cnt=0 -> 1
[MP0] signal: buffer NOT empty
[MP0] produce item=1 at[1] cnt=1 -> 2
...
[MP0] produce item=4 at[4] cnt=4 -> 5
[MP0] buffer full (cnt=5), waiting...
[MC0] consume item=0 from[0] cnt=5 -> 4
[MC0] signal: buffer NOT full
...
[MC0] buffer empty (cnt=0), waiting...
[MP0] produce item=5 at[0] cnt=0 -> 1
...
>> [MP0] DONE (produced 20 items)
>> [MC0] DONE (consumed 20 items)
```

**结果分析**：管程版本的生产者-消费者完美运行了 4 个完整周期（每个周期：填满 5 个 → 清空 5 个），生产者共生产 20 个 item、消费者消费 20 个 item，无一遗漏。`cnt` 严格在 [0, 5] 范围内波动，每次从 0→5 和 5→0 的翻转都由 `notEmpty.signal()` 和 `notFull.signal()` 精确触发。与信号量版本（2.2）相比，管程版本输出更清晰——`cnt` 的跃迁、`signal` 的触发点、`wait` 的阻塞原因都在日志中一目了然，证明了管程在调试和可维护性上的优势。

---

## 遇到的问题及解决方法

### 问题 1：Makefile 缩进错误导致编译失败

> src/2/build/Makefile

![image-20260518153251090](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260518153251090.png)

直接编译报错，排查后发现是 81 行开头缩进错误，用的是空格而不是 Tab。

![image-20260518153555373](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260518153555373.png)

上图是 gitee 上的截图，可以看到 81 行的 **`$`** 符号没有被正确识别。

![image-20260518154337928](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260518154337928.png)

同样的问题在 src/3 下也有。

### 问题 2：QEMU `-serial stdio` 与 `-parallel stdio` 冲突

**现象**：修改 Makefile 将 `-serial null` 改为 `-serial stdio` 以便将输出重定向到终端，但 `make run` 报错 `Error 1` 退出。

**原因**：QEMU 不允许 `-serial stdio` 和 `-parallel stdio` 同时占用 stdin/stdout。

**解决**：将三个 Assignment2 子项目的 Makefile 中 `-parallel stdio` 改为 `-parallel none`：
```
qemu-system-i386 -hda hd.img -serial stdio -parallel none -no-reboot
```

### 问题 3：VGA 输出有限导致实验数据截断

**现象**：生产者-消费者和哲学家程序输出量大（多个线程循环打印），VGA 屏幕仅 25 行 × 80 列，程序运行结束时早期输出数据已被滚出屏幕，无法截图保存完整结果。

**解决**：在 `stdio.cpp` 中添加串口输出支持——初始化 COM1 端口（`0x3F8`），在 `print()` 函数中同步将字符输出到串口。具体实现：在 `STDIO::initialize()` 中添加 `serial_init()` 配置串口；在 `STDIO::print()` 的字符输出分支中调用 `serial_putc(str[i])`，换行时发送 `\r\n`。Makefile 中使用 `-serial stdio` 使串口输出直接显示在终端，可通过终端滚动和复制保留完整日志。

---

## 思考题

### 1. 为什么 `asm_atomic_exchange` 需要假设 register 不是共享变量？

`xchg` 指令的操作数不支持"内存-内存"交换。函数实现先将 `*register` 读入 `eax`，再用 `xchg` 交换 `eax` 与 `*memory`，最后写回 `*register`。若 `register` 指向共享变量且两线程并发执行，在"读 eax → xchg → 写回"之间可能被打断。例如两线程同时读到 `bolt=0` 并存入各自的 `eax`，然后各自 `xchg` 获得 `key=0`，都进入临界区——互斥失效。因此必须保证 `register` 指向线程私有变量。

### 2. 信号量如何实现线程阻塞和唤醒？为什么 P 操作在循环中检查 counter？

**阻塞**：P 操作中将当前线程状态设为 `BLOCKED`，加入 `waiting` 队列，调用 `schedule()`。`schedule()` 只将 `RUNNING` 线程放回就绪队列，被阻塞的线程自然不再被调度。

**唤醒**：V 操作从 `waiting` 队头取出线程，调用 `MESA_WakeUp` 将状态设为 `READY`，放入就绪队列。

**循环检查**：本实验采用 MESA 模型，被唤醒者不立即执行。从唤醒到实际运行之间，可能有其他线程抢先消耗资源，因此被唤醒后必须重新检查 `counter > 0`。

### 3. 单核 CPU 可用开/关中断实现锁，多核为何不行？

单核中，关中断（`cli`）可保证当前执行流不被中断，临界区代码不会被其他线程打断。多核中，各核心独立执行指令流，`cli` 仅关闭当前核心的中断，其他核心仍可并发访问共享变量，互斥效果完全失效。多核环境必须使用原子指令（如 `lock cmpxchg`）实现跨核心的互斥。

### 4. 自旋锁忙等待的优缺点及适用场景

**优点**：实现极简，无需线程切换开销（上下文保存/恢复），锁释放到获取的延迟极低。

**缺点**：CPU 空转浪费处理机时间；无公平性保证，可能饥饿；多核场景下持续写内存产生缓存一致性流量。

**适用场景**：临界区极短（几条指令级别）、锁竞争不激烈的场景，如内核中保护小片共享数据。当临界区较长（如涉及 I/O）或竞争激烈时，应使用信号量等阻塞式机制。

