#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"

STDIO stdio;
InterruptManager interruptManager;
ProgramManager programManager;

// ============ 管程数据结构 ============
#define BUFFER_SIZE 5
#define TOTAL_ITEMS 20

MonitorMutex monLock;           // 管程互斥锁
MonitorCondition notFull;       // "缓冲区非满"条件
MonitorCondition notEmpty;      // "缓冲区非空"条件

int buffer[BUFFER_SIZE];
int in = 0, out = 0, count = 0;

// ============ 生产者（管程版本）============
void monitor_producer(void *arg)
{
    int id = (int)arg;
    for (int i = 0; i < TOTAL_ITEMS; i++)
    {
        // ① 进入管程
        monLock.acquire();

        // ② 条件等待：缓冲区满则阻塞
        while (count == BUFFER_SIZE)
        {
            printf("[MP%d] buffer full (cnt=%d), waiting...\n", id, count);
            notFull.wait(&monLock);
        }

        // ③ 放入产品
        int item = id * 100 + i;
        buffer[in] = item;
        printf("[MP%d] produce item=%d at[%d] cnt=%d -> %d\n",
               id, item, in, count, count + 1);
        in = (in + 1) % BUFFER_SIZE;
        count++;

        // ④ 通知消费者
        if (count == 1)
        {
            printf("[MP%d] signal: buffer NOT empty\n", id);
            notEmpty.signal();
        }

        // ⑤ 离开管程
        monLock.release();

        // 模拟处理时间
        int delay = 0xfffff;
        while (delay) --delay;
    }
    printf(">> [MP%d] DONE (produced %d items)\n", id, TOTAL_ITEMS);
}

// ============ 消费者（管程版本）============
void monitor_consumer(void *arg)
{
    int id = (int)arg;
    for (int i = 0; i < TOTAL_ITEMS; i++)
    {
        // ① 进入管程
        monLock.acquire();

        // ② 条件等待：缓冲区空则阻塞
        while (count == 0)
        {
            printf("[MC%d] buffer empty (cnt=%d), waiting...\n", id, count);
            notEmpty.wait(&monLock);
        }

        // ③ 取出产品
        int item = buffer[out];
        printf("[MC%d] consume item=%d from[%d] cnt=%d -> %d\n",
               id, item, out, count, count - 1);
        out = (out + 1) % BUFFER_SIZE;
        count--;

        // ④ 通知生产者
        if (count == BUFFER_SIZE - 1)
        {
            printf("[MC%d] signal: buffer NOT full\n", id);
            notFull.signal();
        }

        // ⑤ 离开管程
        monLock.release();

        // 模拟处理时间
        int delay = 0xfffff;
        while (delay) --delay;
    }
    printf(">> [MC%d] DONE (consumed %d items)\n", id, TOTAL_ITEMS);
}

void first_thread(void *arg)
{
    stdio.moveCursor(0);
    for (int i = 0; i < 25 * 80; ++i) stdio.print(' ');
    stdio.moveCursor(0);

    printf("========================================\n");
    printf("  Monitor: Producer-Consumer Problem\n");
    printf("========================================\n");
    printf("Buffer=%d | 1 Producer + 1 Consumer\n", BUFFER_SIZE);
    printf("Each thread produces/consumes %d items\n", TOTAL_ITEMS);
    printf("Structure: acquire -> wait/signal -> release\n\n");

    // 初始化管程
    monLock.initialize();
    notFull.initialize();
    notEmpty.initialize();

    // 创建线程
    programManager.executeThread(monitor_producer, (void *)0, "mon_prod", 1);
    programManager.executeThread(monitor_consumer, (void *)0, "mon_cons", 1);

    asm_halt();
}

extern "C" void setup_kernel()
{
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    stdio.initialize();
    programManager.initialize();

    int pid = programManager.executeThread(first_thread, nullptr, "first", 1);
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
