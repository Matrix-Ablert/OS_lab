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
    printf("=== 4.1 Page Replacement Test (Clock Algorithm) ===\n\n");

    // 分配超过maxPhysicalPages（48）个页，触发页面置换
    // 先分配45个页，其中一部分模拟访问过
    char *pages[60];
    int i;

    printf("[Phase 1] Allocating 45 pages (simulating access on some)...\n");
    for (i = 0; i < 45; i++)
    {
        pages[i] = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 1);
        if (!pages[i])
        {
            printf("Allocation #%d failed!\n", i + 1);
            break;
        }
        // 模拟访问：每隔2个页标记为已访问（写入一个值）
        if (i % 3 == 0)
        {
            *pages[i] = 0xAA;
            memoryManager.accessPage((int)pages[i]);
        }
        if (i % 20 == 19)
        {
            printf("   ... allocated %d pages so far\n", i + 1);
        }
    }
    printf("[Phase 1] Allocated %d pages. allocatedPhysicalCount=%d\n\n",
           i, memoryManager.allocatedPhysicalCount);

    // 再分配15个页，触发页面置换
    printf("[Phase 2] Allocating 15 more pages (triggers replacement)...\n");
    for (; i < 60; i++)
    {
        pages[i] = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 1);
        if (!pages[i])
        {
            printf("Allocation #%d failed!\n", i + 1);
            break;
        }
        if (i % 5 == 0)
        {
            printf("   ... allocated %d pages total\n", i + 1);
        }
    }
    printf("[Phase 2] Total allocated: %d pages\n\n", i);

    printf("=== Done. System halting. ===\n");
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
