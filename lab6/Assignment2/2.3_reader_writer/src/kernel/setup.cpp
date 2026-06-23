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

RWLock rwLock;
int shared_data = 0;

uint32 my_rand_seed = 12345;

uint32 my_rand()
{
    my_rand_seed = my_rand_seed * 1103515245 + 12345;
    return (my_rand_seed >> 16) & 0x7FFF;
}

void reader_thread(void *arg)
{
    int id = (int)(long long)arg;
    for (int round = 0; round < 10; round++)
    {
        rwLock.readLock();
        printf("[Reader %d] enter  CR, shared_data = %d\n", id, shared_data);

        // 临界区内留较长 delay, 让多个读者时间上重叠
        int delay = my_rand() % 0xFFFFF;
        while (delay)
            --delay;

        printf("[Reader %d] leave  CR\n", id);
        rwLock.readUnlock();

        // 临界区外几乎不 delay — 立刻尝试重入, 保证总有读者在读
        delay = 0xFF;
        while (delay)
            --delay;
    }
    printf("[Reader %d] all done!\n", id);
}

void writer_thread(void *arg)
{
    for (int round = 0; round < 3; round++)
    {
        printf("[Writer] trying to enter CR...\n");
        rwLock.writeLock();

        ++shared_data;
        printf("[Writer] WRITE: shared_data = %d\n", shared_data);

        // 临界区内 delay 缩短, 万一抢到锁也快速释放
        int delay = 0xFFF;
        while (delay)
            --delay;

        printf("[Writer] leave  CR\n");
        rwLock.writeUnlock();

        // 临界区外 delay 保持较大, 给读者足够时间重入
        delay = 0xFFFFF;
        while (delay)
            --delay;
    }
    printf("[Writer] all done!\n");
}

void first_thread(void *arg)
{
    // 第1个线程不可以返回
    stdio.moveCursor(0);
    for (int i = 0; i < 25 * 80; ++i)
    {
        stdio.print(' ');
    }
    stdio.moveCursor(0);

    printf("=== Reader-Writer (Reader-Priority) ===\n");
    printf("5 Readers + 1 Writer -> Writer Starvation\n\n");

    rwLock.initialize();
    shared_data = 0;

    // 先创建全部读者 — 让读者抢占 wrtLock, 造成写者饥饿
    programManager.executeThread(reader_thread, (void *)1, "reader1", 1);
    programManager.executeThread(reader_thread, (void *)2, "reader2", 1);
    programManager.executeThread(reader_thread, (void *)3, "reader3", 1);
    programManager.executeThread(reader_thread, (void *)4, "reader4", 1);
    programManager.executeThread(reader_thread, (void *)5, "reader5", 1);
    // 最后创建写者 — 此时 wrtLock 已被读者持有, 写者阻塞到所有读者结束
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
