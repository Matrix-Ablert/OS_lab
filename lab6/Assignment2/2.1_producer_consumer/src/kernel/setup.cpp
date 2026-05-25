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
int count = 0;

void producer(void *arg)
{
    const char *name = (const char *)arg;
    for (int i = 0; i < 10; i++)
    {
        buffer[in] = i;
        printf("Producer %s: put %d at [%d], buf_count=%d\n",
               name, i, in, count + 1);
        in = (in + 1) % BUFFER_SIZE;
        count++;

        int delay = 0xfffff;
        while (delay)
            --delay;
    }
    printf("Producer %s: done!\n", name);
}

void consumer(void *arg)
{
    const char *name = (const char *)arg;
    for (int i = 0; i < 10; i++)
    {
        int item = buffer[out];
        printf("Consumer %s: get %d from [%d], buf_count=%d\n",
               name, item, out, count - 1);
        out = (out + 1) % BUFFER_SIZE;
        count--;

        int delay = 0xfffff;
        while (delay)
            --delay;
    }
    printf("Consumer %s: done!\n", name);
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

    printf("=== Producer-Consumer WITHOUT Sync ===\n");
    printf("BufSize=%d | 2 Producers + 2 Consumers\n\n", BUFFER_SIZE);

    in = 0;
    out = 0;
    count = 0;

    programManager.executeThread(producer, (void *)"A", "prod_A", 1);
    programManager.executeThread(consumer, (void *)"X", "cons_X", 1);
    programManager.executeThread(producer, (void *)"B", "prod_B", 1);
    programManager.executeThread(consumer, (void *)"Y", "cons_Y", 1);

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
