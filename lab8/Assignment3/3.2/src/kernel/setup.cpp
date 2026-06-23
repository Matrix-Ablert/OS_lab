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

void delay_for_child()
{
    // 简单延迟，让父进程先进入 wait，更容易观察 wait 阻塞/唤醒流程。
    uint32 tmp = 0x3fffff;
    while (tmp)
        --tmp;
}

void first_process()
{
    int retval;
    int child1 = fork();

    if (child1 == -1)
    {
        printf("fork child1 failed\n");
        asm_halt();
    }

    if (child1 == 0)
    {
        // 第一个子进程用 42 退出，父进程 wait 时应读到同样的返回值。
        delay_for_child();
        printf("child pid: %d, exit(42)\n", programManager.running->pid);
        exit(42);
    }

    // 只有父进程会继续执行到这里；child1 分支已经 exit。
    printf("parent pid: %d, child1 pid: %d\n", programManager.running->pid, child1);

    int child2 = fork();

    if (child2 == -1)
    {
        printf("fork child2 failed\n");
        asm_halt();
    }

    if (child2 == 0)
    {
        // 第二个子进程用不同返回值退出，用来验证 wait 的 pid/retval 对应关系。
        delay_for_child();
        printf("child pid: %d, exit(84)\n", programManager.running->pid);
        exit(84);
    }

    // 父进程连续 fork 两个直接子进程后，统一通过 wait 回收。
    printf("parent pid: %d, child2 pid: %d\n", programManager.running->pid, child2);

    int pid;
    // wait 返回 -1 表示当前父进程已经没有可等待的子进程。
    while ((pid = wait(&retval)) != -1)
    {
        printf("wait child pid: %d, retval: %d\n", pid, retval);
    }

    printf("all child processes collected, programs: %d\n",
           programManager.allPrograms.size());
    asm_halt();
}

void first_thread(void *arg)
{

    printf("start process\n");
    // 只启动一个父进程，避免额外内核线程输出影响 3.2 结果截图。
    programManager.executeProcess((const char *)first_process, 1);
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
