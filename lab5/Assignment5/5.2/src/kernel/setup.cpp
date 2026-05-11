#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"

STDIO stdio;
InterruptManager interruptManager;
ProgramManager programManager;

// 全局等待队列和阻塞线程指针
List waitList;
PCB *sleeperPCB = nullptr;

// ==================== Test: thread_sleep / thread_wakeup ====================
void sleeper_thread(void *arg) {
    sleeperPCB = programManager.running;
    printf("[Sleeper] PID=%d START, status=RUNNING\n",
           programManager.running->pid);
    printf("[Sleeper] PID=%d calling thread_sleep -> BLOCKED\n",
           programManager.running->pid);
    thread_sleep(&waitList);
    printf("[Sleeper] PID=%d WOKEN UP! status=READY->RUNNING, exiting\n",
           programManager.running->pid);
}

void waker_thread(void *arg) {
    printf("[Waker] PID=%d START, will wake sleeper after work\n",
           programManager.running->pid);
    // 做若干轮工作（让时钟中断自然驱动调度）
    for (int i = 1; i <= 5; ++i) {
        printf("[Waker] working... %d\n", i);
        for (volatile int d = 0; d < 5000000; ++d);
    }
    if (sleeperPCB) {
        printf("[Waker] Calling thread_wakeup on sleeper(PID=%d)\n",
               sleeperPCB->pid);
        thread_wakeup(sleeperPCB, &waitList);
        printf("[Waker] Sleeper now READY\n");
    }
    printf("[Waker] Done, exiting\n");
}

// 主线程 — 创建sleeper+waker，靠时钟中断自然调度
void first_thread(void *arg)
{
    if (!programManager.running->pid)
    {
        waitList.initialize();
        printf("========== thread_sleep & thread_wakeup Test ==========\n");
        programManager.executeThread(sleeper_thread, nullptr, "sleeper", 1);
        programManager.executeThread(waker_thread, nullptr, "waker", 1);
        printf("Created sleeper and waker. Clock interrupt drives RR...\n");
    }
    // 主线程保持运行（靠时钟中断抢占来给其它线程机会）
    int count = 0;
    while (1) {
        ++count;
        for (volatile int d = 0; d < 5000000; ++d);
        if (count >= 15) {
            printf("========== Test done, halt ==========\n");
            asm_halt();
        }
    }
}

extern "C" void setup_kernel()
{
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);
    stdio.initialize();
    programManager.initialize();

    int pid = programManager.executeThread(first_thread, nullptr, "main", 1);
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
