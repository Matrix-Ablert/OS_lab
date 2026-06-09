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
    printf("========== 4.3 Buddy System Test ==========\n\n");

    // 显示 Buddy System 初始状态
    printf("[Initial Buddy Status]\n");
    memoryManager.printBuddyStatus();

    // ---- Test 1: 分配不同大小的块 ----
    printf("\n--- Test 1: Allocate blocks of various sizes ---\n");

    // 分配 1 页（order 0）
    int a1 = memoryManager.buddyAllocatePages(1);
    printf("Allocated 1 page  at 0x%x (order 0)\n", a1);

    // 分配 2 页（order 1）
    int a2 = memoryManager.buddyAllocatePages(2);
    printf("Allocated 2 pages at 0x%x (order 1)\n", a2);

    // 分配 3 页（向上取整到 order 2 = 4 页）
    int a3 = memoryManager.buddyAllocatePages(3);
    printf("Allocated 3 pages at 0x%x (rounded to order 2)\n", a3);

    // 分配 8 页（order 3）
    int a4 = memoryManager.buddyAllocatePages(8);
    printf("Allocated 8 pages at 0x%x (order 3)\n", a4);

    printf("\n[After Test 1: Buddy Status]\n");
    memoryManager.printBuddyStatus();

    // ---- Test 2: 释放并观察合并 ----
    printf("\n--- Test 2: Release blocks and observe buddy merging ---\n");

    // 释放 2 页块（应回到 order 1 的空闲列表）
    printf("Releasing 2-page block at 0x%x\n", a2);
    memoryManager.buddyReleasePages(a2, 2);

    printf("Releasing 1-page block at 0x%x\n", a1);
    memoryManager.buddyReleasePages(a1, 1);

    printf("\n[After Test 2: Buddy Status]\n");
    memoryManager.printBuddyStatus();

    // ---- Test 3: 大量分配耗尽 buddy 资源 ----
    printf("\n--- Test 3: Allocate many pages (stress test) ---\n");
    int count = 0;
    int allocCount = 0;
    for (int i = 0; i < 1024; ++i)
    {
        int addr = memoryManager.buddyAllocatePages(1);
        if (!addr)
        {
            printf("Buddy exhausted after %d allocations\n", allocCount);
            break;
        }
        ++allocCount;
        if (allocCount % 200 == 0)
        {
            printf("   ... allocated %d single pages\n", allocCount);
        }
    }
    printf("Stress test allocated %d single pages\n", allocCount);

    printf("\n[After Test 3: Buddy Status]\n");
    memoryManager.printBuddyStatus();

    // ---- Summary ----
    printf("\n========== Buddy System Summary ==========\n");
    printf("Compared to bitmap-based allocator:\n");
    printf("  1. Buddy: O(log N) allocation via power-of-2 blocks\n");
    printf("  2. Buddy: Natural buddy merging reduces external fragmentation\n");
    printf("  3. Bitmap: Simple linear scan, O(N) for each allocation\n");
    printf("  4. Bitmap: No fragmentation control\n");
    printf("==========================================\n");

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
