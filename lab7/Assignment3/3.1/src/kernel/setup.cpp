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
    printf("=== 3.1 Alloc/Release Test ===\n");

    int pde_addr, pte_addr, phys_addr;
    char *p1, *p2, *p3, *p4;

    // Step 1: 分配 100 个内核页
    p1 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100);
    pde_addr = memoryManager.toPDE((int)p1);
    pte_addr = memoryManager.toPTE((int)p1);
    phys_addr = memoryManager.vaddr2paddr((int)p1);
    printf("#1 +100 v=%x PDE=%x PTE=%x ph=%x\n", p1, pde_addr, pte_addr, phys_addr);

    // Step 2: 分配 10 个内核页
    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10);
    pde_addr = memoryManager.toPDE((int)p2);
    pte_addr = memoryManager.toPTE((int)p2);
    phys_addr = memoryManager.vaddr2paddr((int)p2);
    printf("#2  +10 v=%x PDE=%x PTE=%x ph=%x\n", p2, pde_addr, pte_addr, phys_addr);

    // Step 3: 再分配 100 个内核页
    p3 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100);
    pde_addr = memoryManager.toPDE((int)p3);
    pte_addr = memoryManager.toPTE((int)p3);
    phys_addr = memoryManager.vaddr2paddr((int)p3);
    printf("#3 +100 v=%x PDE=%x PTE=%x ph=%x\n", p3, pde_addr, pte_addr, phys_addr);

    // Step 4: 释放 p2 的 10 个页
    printf("#4  -10 @%x (released)\n", p2);
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p2, 10);

    // Step 5: 再次分配 100 个页 - 观察是否复用 p2 的虚拟地址
    p4 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100);
    pde_addr = memoryManager.toPDE((int)p4);
    pte_addr = memoryManager.toPTE((int)p4);
    phys_addr = memoryManager.vaddr2paddr((int)p4);
    printf("#5 +100 v=%x PDE=%x PTE=%x ph=%x\n", p4, pde_addr, pte_addr, phys_addr);

    // Step 6: 再分配 10 个页 - 观察最终布局
    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10);
    pde_addr = memoryManager.toPDE((int)p2);
    pte_addr = memoryManager.toPTE((int)p2);
    phys_addr = memoryManager.vaddr2paddr((int)p2);
    printf("#6  +10 v=%x PDE=%x PTE=%x ph=%x\n", p2, pde_addr, pte_addr, phys_addr);

    printf("=== Done ===\n");

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
