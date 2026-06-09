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

// 策略名称表
static const char *strategyNames[] = {"First Fit", "Best Fit", "Worst Fit", "Next Fit"};

// 获取物理页索引
int getPhysicalIndex(int vaddr)
{
    int paddr = memoryManager.vaddr2paddr(vaddr);
    return (paddr - memoryManager.kernelPhysical.startAddress) / PAGE_SIZE;
}

// 打印 bitmap 统计信息
void printBitmapStats()
{
    BitMap &bmap = memoryManager.kernelPhysical.resources;
    printf("  [Used=%d MaxFree=%d Frag=%d]\n",
           bmap.getUsedCount(),
           bmap.getMaxFreeBlock(),
           bmap.getFreeFragmentCount());
}

// 执行单种策略的测试，返回关键分配的物理页索引
// 测试场景: alloc(30,10,20,5) -> free(30,20) -> alloc(8)
// 制造空洞: [0-29 size 30], [40-59 size 20], [65+ huge]
// - First Fit: 选 idx=0
// - Best Fit: 选 idx=40 (size 20 < size 30)
// - Worst Fit: 选 idx=65 (largest)
// - Next Fit: 选 idx=65 (lastIndex=65)
int testOneStrategy(AllocationStrategy strategy)
{
    memoryManager.kernelPhysical.resources.setStrategy(strategy);

    // Step 1: 分配四个块 A(30), B(10), C(20), D(5)
    int pA = memoryManager.allocatePages(AddressPoolType::KERNEL, 30);
    int pB = memoryManager.allocatePages(AddressPoolType::KERNEL, 10);
    int pC = memoryManager.allocatePages(AddressPoolType::KERNEL, 20);
    int pD = memoryManager.allocatePages(AddressPoolType::KERNEL, 5);

    printf("[%s] alloc: 30@%d 10@%d 20@%d 5@%d | ",
           strategyNames[strategy],
           getPhysicalIndex(pA), getPhysicalIndex(pB),
           getPhysicalIndex(pC), getPhysicalIndex(pD));
    printBitmapStats();

    // Step 2: 释放 A(30) 和 C(20)，制造空洞 30 和 20
    memoryManager.releasePages(AddressPoolType::KERNEL, pA, 30);
    memoryManager.releasePages(AddressPoolType::KERNEL, pC, 20);

    printf("[%s] free A(30) C(20) -> holes:30 & 20 | ",
           strategyNames[strategy]);
    printBitmapStats();

    // Step 3: 分配 8 页 —— 关键测试点
    int pE = memoryManager.allocatePages(AddressPoolType::KERNEL, 8);
    int phyIdx = getPhysicalIndex(pE);

    printf("[%s] alloc 8 -> idx=%d | ",
           strategyNames[strategy], phyIdx);
    printBitmapStats();

    // Step 4: 清理
    memoryManager.releasePages(AddressPoolType::KERNEL, pB, 10);
    memoryManager.releasePages(AddressPoolType::KERNEL, pD, 5);
    memoryManager.releasePages(AddressPoolType::KERNEL, pE, 8);

    printf("[%s] cleanup | ", strategyNames[strategy]);
    printBitmapStats();

    return phyIdx;
}

void first_thread(void *arg)
{
    printf("\n");
    printf("===== Lab 2.1: Memory Allocation Strategies =====\n");
    printf(" Test: alloc(30,10,20,5) -> free(30,20) -> alloc(8)\n");
    printf(" Holes: idx[0]=30, idx[40]=20, idx[65+]=huge\n\n");

    int rFF = testOneStrategy(AllocationStrategy::FIRST_FIT);
    int rBF = testOneStrategy(AllocationStrategy::BEST_FIT);
    int rWF = testOneStrategy(AllocationStrategy::WORST_FIT);
    int rNF = testOneStrategy(AllocationStrategy::NEXT_FIT);

    printf("\n========== FINAL SUMMARY ==========\n");
    printf(" Strategy  | alloc(8) idx | Expected\n");
    printf(" ----------|--------------|---------\n");
    printf(" First Fit | %d             | 0  (first)\n", rFF);
    printf(" Best Fit  | %d             | 40 (smallest)\n", rBF);
    printf(" Worst Fit | %d             | 65 (largest)\n", rWF);
    printf(" Next Fit  | %d             | 65 (lastIdx)\n", rNF);
    printf("===================================\n");

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
