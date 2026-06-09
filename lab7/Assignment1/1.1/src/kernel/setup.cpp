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
    printf("===== Test 1: Basic Allocation =====\n\n");

    // --- 内核物理地址池 ---
    printf("--- Kernel Physical Pool ---\n");
    int k1 = memoryManager.allocatePhysicalPages(KERNEL, 10);
    printf("  Allocate 10 pages, start addr: 0x%x\n", k1);

    int k2 = memoryManager.allocatePhysicalPages(KERNEL, 20);
    printf("  Allocate 20 pages, start addr: 0x%x\n", k2);

    int k3 = memoryManager.allocatePhysicalPages(KERNEL, 50);
    printf("  Allocate 50 pages, start addr: 0x%x\n", k3);

    // --- 用户物理地址池 ---
    printf("\n--- User Physical Pool ---\n");
    int u1 = memoryManager.allocatePhysicalPages(USER, 10);
    printf("  Allocate 10 pages, start addr: 0x%x\n", u1);

    int u2 = memoryManager.allocatePhysicalPages(USER, 20);
    printf("  Allocate 20 pages, start addr: 0x%x\n", u2);

    int u3 = memoryManager.allocatePhysicalPages(USER, 50);
    printf("  Allocate 50 pages, start addr: 0x%x\n", u3);

    // --- 地址连续性验证 ---
    printf("\n--- Continuity Verification ---\n");
    printf("  k2 - k1 = 0x%x (expected 0x%x) %s\n",
           k2 - k1, 10 * PAGE_SIZE,
           (k2 - k1 == 10 * PAGE_SIZE) ? "PASS" : "FAIL");
    printf("  k3 - k2 = 0x%x (expected 0x%x) %s\n",
           k3 - k2, 20 * PAGE_SIZE,
           (k3 - k2 == 20 * PAGE_SIZE) ? "PASS" : "FAIL");
    printf("  u2 - u1 = 0x%x (expected 0x%x) %s\n",
           u2 - u1, 10 * PAGE_SIZE,
           (u2 - u1 == 10 * PAGE_SIZE) ? "PASS" : "FAIL");
    printf("  u3 - u2 = 0x%x (expected 0x%x) %s\n",
           u3 - u2, 20 * PAGE_SIZE,
           (u3 - u2 == 20 * PAGE_SIZE) ? "PASS" : "FAIL");

    // ========== Test 2: Release and Reuse ==========
    printf("\n\n===== Test 2: Release and Reuse =====\n\n");

    // 从内核池分配三批页
    int a = memoryManager.allocatePhysicalPages(KERNEL, 10);
    printf("[Alloc] 10 kernel pages at 0x%x\n", a);
    int b = memoryManager.allocatePhysicalPages(KERNEL, 10);
    printf("[Alloc] 10 kernel pages at 0x%x\n", b);
    int c = memoryManager.allocatePhysicalPages(KERNEL, 10);
    printf("[Alloc] 10 kernel pages at 0x%x\n", c);

    // 释放中间的 b
    printf("\n[Release] 10 kernel pages at 0x%x\n", b);
    memoryManager.releasePhysicalPages(KERNEL, b, 10);

    // 重新分配 10 页，应复用 b 的地址（First-Fit）
    int d = memoryManager.allocatePhysicalPages(KERNEL, 10);
    printf("[Realloc] 10 kernel pages at 0x%x\n", d);
    printf("  Reuse check: expected 0x%x, got 0x%x => %s\n",
           b, d, (b == d) ? "PASS" : "FAIL");

    // 分配 5 页，也应落在 b 的区间内
    int e = memoryManager.allocatePhysicalPages(KERNEL, 5);
    printf("[Alloc] 5 kernel pages at 0x%x (should be >= 0x%x)\n", e, b);

    // 从用户池做同样的释放复用测试
    printf("\n--- User Pool reuse test ---\n");
    int ua = memoryManager.allocatePhysicalPages(USER, 10);
    printf("[Alloc] 10 user pages at 0x%x\n", ua);
    int ub = memoryManager.allocatePhysicalPages(USER, 10);
    printf("[Alloc] 10 user pages at 0x%x\n", ub);
    int uc = memoryManager.allocatePhysicalPages(USER, 10);
    printf("[Alloc] 10 user pages at 0x%x\n", uc);

    printf("\n[Release] 10 user pages at 0x%x\n", ub);
    memoryManager.releasePhysicalPages(USER, ub, 10);

    int ud = memoryManager.allocatePhysicalPages(USER, 10);
    printf("[Realloc] 10 user pages at 0x%x\n", ud);
    printf("  Reuse check: expected 0x%x, got 0x%x => %s\n",
           ub, ud, (ub == ud) ? "PASS" : "FAIL");

    printf("\n===== All Tests Done =====\n");

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
