/**
 * setup.cpp - 读者优先的读者-写者问题测试
 *
 * sync.cpp 中的 RWLock 是经典读者优先算法：
 *   1. 第一个读者进入时会占用 wrtLock，把写者挡在外面。
 *   2. 只要 readCount > 0，后来的读者只需要更新 readCount，
 *      不需要等待已经阻塞的写者，因此可以继续“插队”进入。
 *
 * 本测试通过 6 个读者反复“进入-退出-再进入”，让 1 个写者长时间等待，
 * 从输出中的 bypassCount 直接观察写者饥饿。
 */

#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;

const int READER_COUNT = 6;      // 读者数量：多个读者交错运行才能体现读者优先
const int READER_ROUNDS = 40;    // 每个读者重复读 40 轮，制造持续读者流

RWLock rwLock;                   // 读者优先读写锁，具体算法在 sync.cpp
int shared_data = 0;             // 共享资源：写者修改，读者读取
volatile int writerWaiting = 0;  // 写者是否已经开始等待，用于判断读者是否在插队
volatile int bypassCount = 0;    // 写者等待期间，读者成功进入临界区的次数

/**
 * 简易伪随机数生成器（LCG 线性同余法）
 * ---------------------------------------------------------------
 * 公式: seed = seed * 1103515245 + 12345
 * 这是 glibc 中 rand() 使用的经典参数，周期长、分布均匀。
 * 在内核环境下不能依赖标准库，所以手写一个最简实现。
 * ---------------------------------------------------------------
 */
uint32 my_rand_seed = 12345;

uint32 my_rand()
{
    my_rand_seed = my_rand_seed * 1103515245 + 12345;
    return (my_rand_seed >> 16) & 0x7FFF;
}

// 短自旋，不主动让出 CPU。用于写者等待后让读者快速重入，形成持续读者流。
void spin_delay(uint32 ticks)
{
    volatile uint32 i = ticks;
    while (i)
    {
        --i;
    }
}

// 自旋后主动调度一次，让其他线程有机会运行，从而让 6 个读者交错进入。
void busy_wait(uint32 ticks)
{
    volatile uint32 i = ticks;
    while (i)
    {
        --i;
    }
    programManager.schedule();
}

void reader_thread(void *arg)
{
    int id = (int)(long long)arg;
    for (int round = 1; round <= READER_ROUNDS; ++round)
    {
        // 读者优先锁的关键：即使写者已经在等待，只要当前还有读者，
        // 新读者仍然可以进入读临界区。
        rwLock.readLock();

        if (writerWaiting)
        {
            // 这里的计数就是实验截图中证明“写者饥饿”的直接证据。
            ++bypassCount;
            printf("[Reader %d] bypass writer, round=%d, bypassCount=%d\n",
                   id, round, bypassCount);
        }

        printf("[Reader %d] enter  CR, round=%d, shared_data=%d\n",
               id, round, shared_data);
        // 模拟读操作耗时，并主动让出 CPU，使其他读者能重叠进入。
        busy_wait(0x80 + (my_rand() % 0x80));
        printf("[Reader %d] leave  CR, round=%d\n", id, round);

        rwLock.readUnlock();

        if (writerWaiting)
        {
            // 写者等待后，读者离开临界区外只做短暂等待，马上尝试重入。
            // 这样 readCount 很难降到 0，写者就会长时间拿不到 wrtLock。
            spin_delay(0x100 + (my_rand() % 0x200));
        }
        else
        {
            // 写者尚未等待时，稍微让出 CPU，保证各个读者都能先启动起来。
            busy_wait(0x40 + (my_rand() % 0x40));
        }
    }
    printf("[Reader %d] all done!\n", id);
}

void writer_thread(void *arg)
{
    // 标记写者开始等待。之后每个仍能进入临界区的读者都会增加 bypassCount。
    writerWaiting = 1;
    printf("[Writer] waiting for write lock...\n");

    // 如果 readCount > 0，写者会阻塞在这里，直到最后一个读者离开。
    rwLock.writeLock();
    writerWaiting = 0;

    // 只有读者全部退出后，写者才会执行到这里。
    ++shared_data;
    printf("[Writer] WRITE after %d reader bypasses, shared_data=%d\n",
           bypassCount, shared_data);
    busy_wait(0x80);

    printf("[Writer] leave  CR\n");
    rwLock.writeUnlock();
    printf("[Writer] all done!\n");
}

void first_thread(void *arg)
{
    // 清屏
    stdio.moveCursor(0);
    for (int i = 0; i < 25 * 80; ++i)
    {
        stdio.print(' ');
    }
    stdio.moveCursor(0);

    printf("=== Reader-Writer (Reader-Priority) ===\n");
    printf("%d Readers + 1 Writer -> Writer Starvation\n\n", READER_COUNT);

    rwLock.initialize();
    shared_data = 0;
    writerWaiting = 0;
    bypassCount = 0;

    // 先创建读者，再创建写者：让读者先占住 wrtLock，便于稳定展示饥饿。
    const char *readerNames[READER_COUNT] = {
        "reader1", "reader2", "reader3", "reader4", "reader5", "reader6"};
    for (int i = 0; i < READER_COUNT; ++i)
    {
        programManager.executeThread(reader_thread, (void *)(i + 1), readerNames[i], 1);
    }
    programManager.executeThread(writer_thread, nullptr, "writer", 1);

    asm_halt();
}

extern "C" void setup_kernel()
{

    // 中断管理器
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    // 输出管理器
    stdio.initialize();

    // 进程/线程管理器
    programManager.initialize();

    // 创建第一个线程
    int pid = programManager.executeThread(first_thread, nullptr, "first thread", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }

    ListItem *item = programManager.readyPrograms.front();
    PCB *firstThread = ListItem2PCB(item, tagInGeneralList);
    firstThread->status = RUNNING;
    programManager.readyPrograms.pop_front();
    programManager.running = firstThread;
    asm_switch_thread(0, firstThread);

    asm_halt();
}
