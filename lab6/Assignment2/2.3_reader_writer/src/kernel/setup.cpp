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
    for (int round = 0; round < 5; round++)
    {
        rwLock.readLock();
        printf("[Reader %d] enter  CR, shared_data = %d\n", id, shared_data);

        int delay = my_rand() % 0xFFFFF;
        while (delay)
            --delay;

        printf("[Reader %d] leave  CR\n", id);
        rwLock.readUnlock();

        delay = my_rand() % 0xFFFFF;
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

        int delay = 0xFFFFF;
        while (delay)
            --delay;

        printf("[Writer] leave  CR\n");
        rwLock.writeUnlock();

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

    programManager.executeThread(writer_thread, nullptr, "writer", 1);
    programManager.executeThread(reader_thread, (void *)1, "reader1", 1);
    programManager.executeThread(reader_thread, (void *)2, "reader2", 1);
    programManager.executeThread(reader_thread, (void *)3, "reader3", 1);
    programManager.executeThread(reader_thread, (void *)4, "reader4", 1);
    programManager.executeThread(reader_thread, (void *)5, "reader5", 1);

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
