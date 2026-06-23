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

void delay_ticks(uint32 count)
{
    while (count)
        --count;
}

void first_process()
{
    int child1 = fork();

    if (child1 == -1)
    {
        printf("fork child1 failed\n");
        asm_halt();
    }

    if (child1 == 0)
    {
        // child1 立即退出；父进程不 wait，因此它会先变成僵尸进程。
        printf("child1 pid: %d, exit(11)\n", programManager.running->pid);
        exit(11);
    }

    // 给 child1 足够多的时钟片先退出，稳定制造“父进程未 wait 的 DEAD 子进程”。
    delay_ticks(0x4ffffff);
    printf("parent pid: %d, child1 pid: %d\n", programManager.running->pid, child1);

    int child2 = fork();

    if (child2 == -1)
    {
        printf("fork child2 failed\n");
        asm_halt();
    }

    if (child2 == 0)
    {
        // child2 延迟退出；父进程会先退出，因此它会被托管给 reaper。
        delay_ticks(0xffffff);
        printf("child2 pid: %d, exit(22)\n", programManager.running->pid);
        exit(22);
    }

    printf("parent pid: %d, child2 pid: %d\n", programManager.running->pid, child2);
    printf("parent pid: %d, exit without wait\n", programManager.running->pid);
    exit(99);
}

void observer_thread(void *arg)
{
    // 等待父进程和两个子进程都经历退出/回收路径后，再打印最终状态。
    delay_ticks(0x3ffffff);
    printf("observer: cleanup finished, programs: %d\n",
           programManager.allPrograms.size());
    asm_halt();
}

void first_thread(void *arg)
{

    printf("start zombie/orphan test\n");
    programManager.executeProcess((const char *)first_process, 1);
    programManager.executeThread(observer_thread, nullptr, "observer", 1);
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
