#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"
#include "syscall.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;
// 内存管理器
MemoryManager memoryManager;
// 系统调用
SystemService systemService;

int syscall_0(int first, int second, int third, int forth, int fifth)
{
    printf("systerm call 0: %d, %d, %d, %d, %d\n",
           first, second, third, forth, fifth);
    return first + second + third + forth + fifth;
}

void first_thread(void *arg)
{
    int a = 123, b = 456;
    int m = max(a, b);
    printf("max(%d, %d) = %d\n", a, b, m);

    int n = 5;
    int f = factorial(n);
    printf("factorial(%d) = %d\n", n, f);

    // 多测几组数据验证正确性
    printf("max(999, 1) = %d\n", max(999, 1));
    printf("max(-5, -10) = %d\n", max(-5, -10));
    printf("factorial(0) = %d\n", factorial(0));
    printf("factorial(1) = %d\n", factorial(1));
    printf("factorial(7) = %d\n", factorial(7));

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

    // 内存管理器
    memoryManager.initialize();

    // 初始化系统调用
    systemService.initialize();
    // 设置0号系统调用
    systemService.setSystemCall(0, (int)syscall_0);
    // 设置1号系统调用: max
    systemService.setSystemCall(1, (int)syscall_max);
    // 设置2号系统调用: factorial
    systemService.setSystemCall(2, (int)syscall_factorial);

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
