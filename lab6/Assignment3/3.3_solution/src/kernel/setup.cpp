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

#define N 5  // 5个哲学家

Semaphore chopstick[N];     // 5根筷子信号量
Semaphore limit;             // ★ 限制同时最多4人拿筷子，破坏循环等待
int eat_count[N] = {0};     // 记录每位哲学家吃了多少次

void philosopher(void *arg)
{
    int id = (int)arg;
    int left = id;                 // 左边筷子编号
    int right = (id + 1) % N;      // 右边筷子编号

    for (int round = 0; round < 10; round++)
    {
        // ① 思考
        printf("[P%d] is Thinking\n", id);
        int delay = 0xfffff;
        while (delay) --delay;

        // ② 饥饿
        printf("[P%d] is Hungry\n", id);

        // ★ 获取"拿筷子许可"：最多允许4人同时尝试拿筷子
        limit.P();


        // 拿左筷子
        chopstick[left].P();
        printf("[P%d] picked up left chopstick[%d]\n", id, left);


        // 巨大延时循环，确保跨越多个时间片
        int deadlock_delay = 0x20000000;
        while (deadlock_delay) --deadlock_delay;

        // 拿右筷子
        chopstick[right].P();
        printf("[P%d] picked up right chopstick[%d]\n", id, right);

        // ③ 吃饭（拿到两根筷子，一定能吃饭）
        eat_count[id]++;
        printf("[P%d] is Eating (count=%d)\n", id, eat_count[id]);
        delay = 0xfffff;
        while (delay) --delay;

        // ④ 放筷子
        chopstick[right].V();
        chopstick[left].V();
        printf("[P%d] put down chopsticks\n\n", id);

        // ★ 释放"拿筷子许可"
        limit.V();
    }
    printf("[P%d] done! total eat count=%d\n", id, eat_count[id]);
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

    printf("=== Dining Philosophers (Deadlock-Free Solution) ===\n");
    printf("5 philosophers, 5 chopsticks (semaphores)\n");
    printf("Strategy: limit max 4 philosophers holding chopsticks\n");
    printf("This breaks the 'circular wait' condition!\n\n");

    // 初始化筷子信号量（每根筷子初始=1）
    for (int i = 0; i < N; i++)
    {
        chopstick[i].initialize(1);
    }
    // limit 初始值=4，即最多4人可同时持有筷子
    limit.initialize(4);

    // 创建5个哲学家线程
    programManager.executeThread(philosopher, (void *)0, "P0", 1);
    programManager.executeThread(philosopher, (void *)1, "P1", 1);
    programManager.executeThread(philosopher, (void *)2, "P2", 1);
    programManager.executeThread(philosopher, (void *)3, "P3", 1);
    programManager.executeThread(philosopher, (void *)4, "P4", 1);

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
