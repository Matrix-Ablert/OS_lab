#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;
// 内存管理器
MemoryManager memoryManager;

void first_thread(void *arg)
{
    printf("=== 3.3 Vaddr to Paddr Verification ===\n");

    // Step 1: 分配 1 个内核页
    char *va = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 1);
    if (!va)
    {
        printf("allocatePages failed!\n");
        asm_halt();
    }

    // Step 2: 计算虚拟地址对应的物理地址并打印
    int pa = memoryManager.vaddr2paddr((int)va);
    printf("#1 Allocated vaddr: 0x%x\n", va);
    printf("#2 vaddr2paddr -> phy: 0x%x\n", pa);

    // Step 3: 向虚拟地址写入特定值 0xDEADBEEF
    *(int *)va = 0xDEADBEEF;
    printf("#3 Wrote 0xDEADBEEF to *(%x)=%x\n", va, *(int *)va);
    printf("    (verify via QEMU Monitor: xp 0x%x)\n", pa);

    printf("=== Done (looping for monitor) ===\n");

    // Step 4: 无限循环，保持 QEMU 响应以便 Monitor 验证
    while (1)
    {
        asm_halt();
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

    // 进程/线程管理器
    programManager.initialize();

    // 内存管理器
    memoryManager.openPageMechanism();
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
    firstThread->status = RUNNING;
    programManager.readyPrograms.pop_front();
    programManager.running = firstThread;
    asm_switch_thread(0, firstThread);

    asm_halt();
}
