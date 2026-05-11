#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;

void third_thread(void *arg) {
    printf("[%s] PID=%d Priority=%d START\n",
           programManager.running->name, programManager.running->pid,
           programManager.running->priority);
    for (int i = 1; i <= 3; ++i) {
        printf("[%s] iter=%d ticks_left=%d\n",
               programManager.running->name, i, programManager.running->ticks);
        for (volatile int d = 0; d < 5000000; ++d);
    }
    printf("[%s] EXIT\n", programManager.running->name);
}

void second_thread(void *arg) {
    printf("[%s] PID=%d Priority=%d START\n",
           programManager.running->name, programManager.running->pid,
           programManager.running->priority);
    for (int i = 1; i <= 3; ++i) {
        printf("[%s] iter=%d ticks_left=%d\n",
               programManager.running->name, i, programManager.running->ticks);
        for (volatile int d = 0; d < 5000000; ++d);
    }
    printf("[%s] EXIT\n", programManager.running->name);
}

void first_thread(void *arg)
{
    // 第1个线程不可以返回(pid=0 退出会halt系统)
    printf("[%s] PID=%d Priority=%d START (main)\n",
           programManager.running->name, programManager.running->pid,
           programManager.running->priority);
    if (!programManager.running->pid)
    {
        int pid2 = programManager.executeThread(second_thread, nullptr, "second_thread", 1);
        int pid3 = programManager.executeThread(third_thread, nullptr, "third_thread", 1);
        printf("[%s] Created child threads: PID=%d, PID=%d\n",
               programManager.running->name, pid2, pid3);
    }
    for (int i = 1; i <= 5; ++i) {
        printf("[%s] iter=%d ticks_left=%d\n",
               programManager.running->name, i, programManager.running->ticks);
        for (volatile int d = 0; d < 5000000; ++d);
    }
    printf("[%s] Done, halt system\n", programManager.running->name);
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
