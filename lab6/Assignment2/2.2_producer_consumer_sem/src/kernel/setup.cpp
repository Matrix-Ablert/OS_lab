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

#define BUFFER_SIZE 5

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;

Semaphore empty;
Semaphore full;
Semaphore mutex;

void producer_s(void *arg)
{
    const char *name = (const char *)arg;
    for (int i = 0; i < 10; i++)
    {
        empty.P();
        mutex.P();

        buffer[in] = i;
        printf("[P-%s] put item %d at slot [%d]\n", name, i, in);
        in = (in + 1) % BUFFER_SIZE;

        mutex.V();
        full.V();

        int delay = 0xfffff;
        while (delay)
            --delay;
    }
    printf("[P-%s] done!\n", name);
}

void consumer_s(void *arg)
{
    const char *name = (const char *)arg;
    for (int i = 0; i < 10; i++)
    {
        full.P();
        mutex.P();

        int item = buffer[out];
        printf("[C-%s] get item %d from slot [%d]\n", name, item, out);
        out = (out + 1) % BUFFER_SIZE;

        mutex.V();
        empty.V();

        int delay = 0xfffff;
        while (delay)
            --delay;
    }
    printf("[C-%s] done!\n", name);
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

    printf("=== Producer-Consumer with Semaphore ===\n");
    printf("BufSize=%d | Sem: empty=%d full=%d mutex=1\n\n",
           BUFFER_SIZE, BUFFER_SIZE);

    in = 0;
    out = 0;
    empty.initialize(BUFFER_SIZE);
    full.initialize(0);
    mutex.initialize(1);

    programManager.executeThread(producer_s, (void *)"A", "pA", 1);
    programManager.executeThread(consumer_s, (void *)"X", "cX", 1);
    programManager.executeThread(producer_s, (void *)"B", "pB", 1);
    programManager.executeThread(consumer_s, (void *)"Y", "cY", 1);

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
