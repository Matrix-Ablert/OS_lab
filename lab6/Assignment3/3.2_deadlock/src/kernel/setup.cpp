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

Semaphore chopstick[N];   // 5根筷子信号量
int eat_count[N] = {0};   // 记录每位哲学家吃了多少次

void philosopher(void *arg)
{
    int id = (int)arg;
    int left = id;                 // 左边筷子编号
    int right = (id + 1) % N;      // 右边筷子编号

    // 只跑几轮，死锁会阻止完成
    for (int round = 0; round < 5; round++)
    {
        // ① 思考 (极短，让所有哲学家尽快进入饥饿状态)
        printf("[P%d] is Thinking\n", id);
        int delay = 0xffff;
        while (delay) --delay;

        // ② 饥饿：拿左筷子
        printf("[P%d] is Hungry\n", id);
        chopstick[left].P();
        printf("[P%d] picked up left chopstick[%d]\n", id, left);

        // ★★★ 关键：两次P()之间插入巨量延时 ★★★
        // 在此延时期间，时钟中断会触发多次调度，
        // 所有没持有筷子的哲学家都会轮流被调度执行，
        // 各自拿起左筷子后再进入各自的延时。
        // 最后所有5人都持有左筷子，再一起去抢右筷子 → 死锁！
        printf("[P%d] waiting for right chopstick[%d]...\n", id, right);

        // 巨大延时循环，确保跨越多个时间片
        int deadlock_delay = 0x20000000;
        while (deadlock_delay) --deadlock_delay;

        // 拿右筷子 —— 这里是死锁触发点！
        chopstick[right].P();
        printf("[P%d] picked up right chopstick[%d]\n", id, right);

        // ③ 吃饭（死锁时无法到达此处）
        eat_count[id]++;
        printf("[P%d] is Eating (count=%d)\n", id, eat_count[id]);
        delay = 0xfffff;
        while (delay) --delay;

        // ④ 放筷子
        chopstick[right].V();
        chopstick[left].V();
        printf("[P%d] put down chopsticks\n\n", id);
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

    printf("=== Dining Philosophers (Deadlock Demo) ===\n");
    printf("5 philosophers, 5 chopsticks (semaphores)\n");
    printf("Strategy: pick left first, then wait,");
    printf(" then pick right\n");
    printf("Deliberately causes deadlock!\n\n");

    // 初始化筷子信号量（每根筷子初始=1）
    for (int i = 0; i < N; i++)
    {
        chopstick[i].initialize(1);
    }

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
