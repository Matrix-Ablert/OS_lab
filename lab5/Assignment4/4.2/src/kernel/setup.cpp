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

// 在指定行写内容（含优先级信息）
void write_row(int row, const char *label, int pid, int pri, int count, uint8 color) {
    char buf[80];
    int pos = 0;

    const char *s = label;
    while (*s) buf[pos++] = *s++;

    buf[pos++] = ' ';
    buf[pos++] = 'P';
    buf[pos++] = 'I';
    buf[pos++] = 'D';
    buf[pos++] = ':';

    char tmp[12];
    itos(tmp, (uint32)pid, 10);
    for (int i = 0; tmp[i]; ++i) buf[pos++] = tmp[i];

    buf[pos++] = ' ';
    buf[pos++] = 'p';
    buf[pos++] = 'r';
    buf[pos++] = 'i';
    buf[pos++] = ':';

    itos(tmp, (uint32)pri, 10);
    for (int i = 0; tmp[i]; ++i) buf[pos++] = tmp[i];

    buf[pos++] = ' ';
    buf[pos++] = 'c';
    buf[pos++] = 'n';
    buf[pos++] = 't';
    buf[pos++] = ':';

    itos(tmp, (uint32)count, 10);
    for (int i = 0; tmp[i]; ++i) buf[pos++] = tmp[i];

    while (pos < 80) buf[pos++] = ' ';
    for (int i = 0; i < 80; ++i) {
        stdio.print(row, i, buf[i], color);
    }
}

// 线程A (pri=3) — 第7行，红色（RR下与其它线程轮流执行）
void thread_A(void *arg) {
    int count = 0;
    while (1) {
        ++count;
        write_row(7, "[RR] ThreadA(pri=3)", programManager.running->pid, 3, count, 0x04);
        for (volatile int d = 0; d < 2000000; ++d);
    }
}

// 线程B (pri=2) — 第5行，青色
void thread_B(void *arg) {
    int count = 0;
    while (1) {
        ++count;
        write_row(5, "[RR] ThreadB(pri=2)", programManager.running->pid, 2, count, 0x0B);
        for (volatile int d = 0; d < 2000000; ++d);
    }
}

// 线程C (pri=1) — 第3行，绿色
void thread_C(void *arg) {
    int count = 0;
    while (1) {
        ++count;
        write_row(3, "[RR] ThreadC(pri=1)", programManager.running->pid, 1, count, 0x0A);
        for (volatile int d = 0; d < 2000000; ++d);
    }
}

// 主线程 (pid=0) — 第1行，白色，创建3个子线程
void first_thread(void *arg)
{
    if (!programManager.running->pid)
    {
        int pid1 = programManager.executeThread(thread_A, nullptr, "thread_A", 3);
        int pid2 = programManager.executeThread(thread_B, nullptr, "thread_B", 2);
        int pid3 = programManager.executeThread(thread_C, nullptr, "thread_C", 1);
        printf("[RR] Created: A(pri=3,PID=%d) B(pri=2,PID=%d) C(pri=1,PID=%d)\n",
               pid1, pid2, pid3);
    }
    int count = 0;
    while (1) {
        ++count;
        write_row(1, "[RR] Main(pri=1)  ", programManager.running->pid, 1, count, 0x07);
        for (volatile int d = 0; d < 2000000; ++d);
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

    // 打印标题
    printf("=== Round-Robin Scheduling Demo ===\n");
    printf("Row1: Main Row3: ThreadC(pri=1) Row5: ThreadB(pri=2) Row7: ThreadA(pri=3)\n");
    printf("Note: RR ignores priority — all threads get equal time slices\n");

    // 进程/线程管理器
    programManager.initialize();

    // 创建第一个线程
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
