#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"
#include "syscall.h"
#include "tss.h"

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
// Task State Segment
TSS tss;

int syscall_0(int first, int second, int third, int forth, int fifth)
{
    printf("systerm call 0: %d, %d, %d, %d, %d\n",
           first, second, third, forth, fifth);
    return first + second + third + forth + fifth;
}

void process_a()
{
    // 使用getpid系统调用获取当前进程pid，再通过sleep阻塞等待唤醒。
    printf("process A start, pid: %d\n", getpid());
    sleep(20);
    printf("process A wake, pid: %d\n", getpid());
    exit(0);
}

void process_b()
{
    // sleep ticks更长，预期会在A之后被唤醒。
    printf("process B start, pid: %d\n", getpid());
    sleep(40);
    printf("process B wake, pid: %d\n", getpid());
    exit(0);
}

void process_c()
{
    // sleep ticks最长，预期最后唤醒，用来展示简单同步效果。
    printf("process C start, pid: %d\n", getpid());
    sleep(60);
    printf("process C wake, pid: %d\n", getpid());
    exit(0);
}

void first_thread(void *arg)
{

    printf("start getpid/sleep test\n");
    // 创建三个用户进程，用不同sleep ticks验证阻塞和自动唤醒。
    programManager.executeProcess((const char *)process_a, 1);
    programManager.executeProcess((const char *)process_b, 1);
    programManager.executeProcess((const char *)process_c, 1);
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

    // 初始化系统调用
    systemService.initialize();
    // 设置0号系统调用
    systemService.setSystemCall(0, (int)syscall_0);
    // 设置1号系统调用
    systemService.setSystemCall(1, (int)syscall_write);
    // 设置2号系统调用
    systemService.setSystemCall(2, (int)syscall_fork);
    // 设置3号系统调用
    systemService.setSystemCall(3, (int)syscall_exit);
    // 设置4号系统调用
    systemService.setSystemCall(4, (int)syscall_wait);
    // 设置5号系统调用
    systemService.setSystemCall(5, (int)syscall_getpid);
    // 设置6号系统调用
    systemService.setSystemCall(6, (int)syscall_sleep);

    // 内存管理器
    memoryManager.initialize();

    // 创建第一个线程
    int pid = programManager.executeThread(first_thread, nullptr, "first thread", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }

    ListItem *item = programManager.readyPrograms.front();
    PCB *firstThread = ListItem2PCB(item, tagInGeneralList);
    firstThread->status = ProgramStatus::RUNNING;
    programManager.readyPrograms.pop_front();
    programManager.running = firstThread;
    asm_switch_thread(0, firstThread);

    asm_halt();
}
