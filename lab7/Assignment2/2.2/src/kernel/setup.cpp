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

static const char *tag;

// Print a labeled data row: [TAG] LABEL v1 v2 ... vN
void printDataRow(const char *label, int *arr, int cnt)
{
    printf("[%s] %s", tag, label);
    for (int i = 1; i <= cnt; ++i)
    {
        if (i == 11) printf(" ");
        printf(" %d", arr[i]);
    }
    printf("\n");
}

void runAnalysis(AllocationStrategy strategy, const char *name)
{
    memoryManager.kernelPhysical.resources.setStrategy(strategy);
    tag = name;

    printf("[%s] =====\n", name);

    int used[16], maxFree[16], frag[16];
    int a[11], cnt = 0;
    BitMap &bm = memoryManager.kernelPhysical.resources;

    // Steps 1-15: allocations and frees
    a[1] = memoryManager.allocatePages(AddressPoolType::KERNEL, 20);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[2] = memoryManager.allocatePages(AddressPoolType::KERNEL, 10);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[3] = memoryManager.allocatePages(AddressPoolType::KERNEL, 30);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[4] = memoryManager.allocatePages(AddressPoolType::KERNEL, 5);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    memoryManager.releasePages(AddressPoolType::KERNEL, a[1], 20);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[5] = memoryManager.allocatePages(AddressPoolType::KERNEL, 15);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    memoryManager.releasePages(AddressPoolType::KERNEL, a[3], 30);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[6] = memoryManager.allocatePages(AddressPoolType::KERNEL, 8);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    memoryManager.releasePages(AddressPoolType::KERNEL, a[2], 10);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[7] = memoryManager.allocatePages(AddressPoolType::KERNEL, 12);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    memoryManager.releasePages(AddressPoolType::KERNEL, a[4], 5);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[8] = memoryManager.allocatePages(AddressPoolType::KERNEL, 7);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[9] = memoryManager.allocatePages(AddressPoolType::KERNEL, 25);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    memoryManager.releasePages(AddressPoolType::KERNEL, a[5], 15);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    a[10] = memoryManager.allocatePages(AddressPoolType::KERNEL, 3);
    used[++cnt]=bm.getUsedCount(); maxFree[cnt]=bm.getMaxFreeBlock(); frag[cnt]=bm.getFreeFragmentCount();

    // Print Step numbers row
    printf("[%s] Step ", tag);
    for (int i = 1; i <= cnt; ++i)
    {
        if (i == 11) printf(" ");
        if (i < 10) printf(" ");
        printf(" %d", i);
    }
    printf("\n");

    // Print operations
    printf("[%s] Op   +20 +10 +30  +5  -20 +15 -30  +8 -10 +12  -5  +7 +25 -15  +3\n", tag);

    // Print stats
    printDataRow("Used ", used, cnt);
    printDataRow("Frag ", frag, cnt);

    // Compact MaxFree - show directly (fits if we remove spaces)
    printf("[%s] MaxFree", tag);
    for (int i = 1; i <= cnt; ++i)
    {
        if (i == 11) printf(" ");
        printf("%d", maxFree[i]);
        if (i < cnt) printf(",");
    }
    printf("\n\n");

    // Cleanup
    int pages[] = {0, 20, 10, 30, 5, 15, 8, 12, 7, 25, 3};
    for (int i = 1; i <= 10; ++i)
        if (a[i] != 0)
            memoryManager.releasePages(AddressPoolType::KERNEL, a[i], pages[i]);
}

void first_thread(void *arg)
{
    printf("\n===== Lab 2.2: Memory Utilization =====\n");
    printf("10 alloc + 5 free: First Fit vs Best Fit\n\n");

    runAnalysis(AllocationStrategy::FIRST_FIT, "FF");
    runAnalysis(AllocationStrategy::BEST_FIT,  "BF");

    printf("========== Summary ==========\n");
    printf("First Fit: first hole | quick\n");
    printf("Best Fit:  smallest hole | saves big blocks\n");
    printf("=============================\n");

    asm_halt();
}

extern "C" void setup_kernel()
{
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    stdio.initialize();
    programManager.initialize();

    memoryManager.openPageMechanism();
    memoryManager.initialize();

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
