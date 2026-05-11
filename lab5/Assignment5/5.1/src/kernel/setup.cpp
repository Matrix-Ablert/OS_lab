#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"

STDIO stdio;
InterruptManager interruptManager;
ProgramManager programManager;

// ==================== Test: thread_yield ====================
void threadB(void *arg) {
    for (int i = 1; i <= 3; ++i) {
        printf("[ThreadB] iter=%d  (yield)\n", i);
        thread_yield();
    }
    printf("[ThreadB] Done, exit\n");
}

void threadA(void *arg) {
    for (int i = 1; i <= 3; ++i) {
        printf("[ThreadA] iter=%d  (yield)\n", i);
        thread_yield();
    }
    printf("[ThreadA] Done, exit\n");
}

// 主线程 — 创建A/B后通过yield让它们交替执行
void first_thread(void *arg)
{
    if (!programManager.running->pid)
    {
        printf("========== thread_yield Test ==========\n");
        programManager.executeThread(threadA, nullptr, "threadA", 1);
        programManager.executeThread(threadB, nullptr, "threadB", 1);
        printf("Created threadA and threadB. Yielding...\n");
    }
    // 通过yield主动让出CPU，让A和B交替运行
    for (int i = 0; i < 8; ++i) {
        thread_yield();
    }
    printf("========== Test done, halt ==========\n");
    asm_halt();
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
