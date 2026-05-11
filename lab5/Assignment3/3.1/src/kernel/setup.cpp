#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "stdlib.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;

// 在指定行写一整行内容（直接操作VGA显存，避免多线程光标冲突）
void write_row(int row, const char *name, int pid, int count, uint8 color) {
    char buf[80];
    int pos = 0;

    // 线程名
    const char *s = name;
    while (*s) buf[pos++] = *s++;
    buf[pos++] = '[';
    buf[pos++] = 'P';
    buf[pos++] = 'I';
    buf[pos++] = 'D';
    buf[pos++] = ':';

    // PID
    char tmp[12];
    itos(tmp, (uint32)pid, 10);
    for (int i = 0; tmp[i]; ++i) buf[pos++] = tmp[i];

    buf[pos++] = ']';
    buf[pos++] = ' ';
    buf[pos++] = 'c';
    buf[pos++] = 'n';
    buf[pos++] = 't';
    buf[pos++] = '=';

    // 计数器
    itos(tmp, (uint32)count, 10);
    for (int i = 0; tmp[i]; ++i) buf[pos++] = tmp[i];

    // 行末填充空格清除残留
    while (pos < 80) buf[pos++] = ' ';

    // 直接写入VGA显存对应行
    for (int i = 0; i < 80; ++i) {
        stdio.print(row, i, buf[i], color);
    }
}

// 线程3 — 第5行，品红色
void thread3_func(void *arg) {
    int count = 0;
    while (1) {
        ++count;
        write_row(5, "Thread3", programManager.running->pid, count, 0x0D);
        for (volatile int d = 0; d < 3000000; ++d);
    }
}

// 线程2 — 第3行，青色
void thread2_func(void *arg) {
    int count = 0;
    while (1) {
        ++count;
        write_row(3, "Thread2", programManager.running->pid, count, 0x0B);
        for (volatile int d = 0; d < 3000000; ++d);
    }
}

// 线程1(主线程,pid=0) — 第1行，绿色，不能退出
void first_thread(void *arg)
{
    if (!programManager.running->pid)
    {
        int pid2 = programManager.executeThread(thread2_func, nullptr, "thread2", 1);
        int pid3 = programManager.executeThread(thread3_func, nullptr, "thread3", 1);
        // 用printf输出一次性的创建信息（仅在启动时，不参与并发竞争）
        printf("Created: %s(PID=%d) at row3, %s(PID=%d) at row5\n",
               "thread2", pid2, "thread3", pid3);
    }

    int count = 0;
    while (1) {
        ++count;
        write_row(1, "Thread1", programManager.running->pid, count, 0x0A);
        for (volatile int d = 0; d < 3000000; ++d);
    }
}

extern "C" void setup_kernel()
{

    // 中断管理器
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    // 输出管理器
    stdio.initialize();

    // 清屏
    for (int r = 0; r < 25; ++r) {
        for (int c = 0; c < 80; ++c) {
            stdio.print(r, c, ' ', 0x07);
        }
    }

    // 进程/线程管理器
    programManager.initialize();

    // 创建第一个线程
    int pid = programManager.executeThread(first_thread, nullptr, "thread1", 1);
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
