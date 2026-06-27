#include "program.h"
#include "stdlib.h"
#include "interrupt.h"
#include "asm_utils.h"
#include "stdio.h"
#include "thread.h"
#include "os_modules.h"
#include "tss.h"
#include "os_constant.h"
#include "process.h"

const int PCB_SIZE = 4096;                   // PCB的大小，4KB。
char PCB_SET[PCB_SIZE * MAX_PROGRAM_AMOUNT]; // 存放PCB的数组，预留了MAX_PROGRAM_AMOUNT个PCB的大小空间。
bool PCB_SET_STATUS[MAX_PROGRAM_AMOUNT];     // PCB的分配状态，true表示已经分配，false表示未分配。
const int REAPER_PID = 0;                    // pid=0 的初始线程(first_thread)作为孤儿进程托管标记。
                                             // 孤儿进程被托管后将 parentPid 改为 0，
                                             // 后续在 schedule() 中由 reaper 自动回收。

ProgramManager::ProgramManager()
{
    initialize();
}

void ProgramManager::initialize()
{
    allPrograms.initialize();
    readyPrograms.initialize();
    running = nullptr;

    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        PCB_SET_STATUS[i] = false;
    }

    // 初始化用户代码段、数据段和栈段
    int selector;

    selector = asm_add_global_descriptor(USER_CODE_LOW, USER_CODE_HIGH);
    USER_CODE_SELECTOR = (selector << 3) | 0x3;

    selector = asm_add_global_descriptor(USER_DATA_LOW, USER_DATA_HIGH);
    USER_DATA_SELECTOR = (selector << 3) | 0x3;

    selector = asm_add_global_descriptor(USER_STACK_LOW, USER_STACK_HIGH);
    USER_STACK_SELECTOR = (selector << 3) | 0x3;

    initializeTSS();
}

int ProgramManager::executeThread(ThreadFunction function, void *parameter, const char *name, int priority)
{
    // 关中断，防止创建线程的过程被打断
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 分配一页作为PCB
    PCB *thread = allocatePCB();

    if (!thread)
        return -1;

    // 初始化分配的页
    memset(thread, 0, PCB_SIZE);

    for (int i = 0; i < MAX_PROGRAM_NAME && name[i]; ++i)
    {
        thread->name[i] = name[i];
    }

    thread->status = ProgramStatus::READY;
    thread->priority = priority;
    thread->ticks = priority * 10;
    thread->ticksPassedBy = 0;
    thread->pid = ((int)thread - (int)PCB_SET) / PCB_SIZE;

    // 线程栈
    thread->stack = (int *)((int)thread + PCB_SIZE - sizeof(ProcessStartStack));
    thread->stack -= 7;
    thread->stack[0] = 0;
    thread->stack[1] = 0;
    thread->stack[2] = 0;
    thread->stack[3] = 0;
    thread->stack[4] = (int)function;
    thread->stack[5] = (int)program_exit;
    thread->stack[6] = (int)parameter;

    allPrograms.push_back(&(thread->tagInAllList));
    readyPrograms.push_back(&(thread->tagInGeneralList));

    // 恢复中断
    interruptManager.setInterruptStatus(status);

    return thread->pid;
}

void ProgramManager::schedule()
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    if (readyPrograms.size() == 0)
    {
        interruptManager.setInterruptStatus(status);
        return;
    }
    else
    {
        //printf("amount of ready programs: %d\n", readyPrograms.size());
    }

    if (running->status == ProgramStatus::RUNNING)
    {
        running->status = ProgramStatus::READY;
        running->ticks = running->priority * 10;
        readyPrograms.push_back(&(running->tagInGeneralList));
    }
    else if (running->status == ProgramStatus::DEAD)
    {
        // 判断是否可立即回收 PCB：
        // 1. !pageDirectoryAddress → 是线程（无用户空间），直接回收。
        // 2. parentPid == REAPER_PID → 是已被托管给 reaper 的孤儿进程，
        //    其父进程已经退出，不会再有人 wait 它，由 schedule 兜底回收。
        if (!running->pageDirectoryAddress || running->parentPid == REAPER_PID)
        {
            releasePCB(running);
        }
        // 普通子进程（pageDirectoryAddress 非空且 parentPid != 0）：
        // 保留 PCB 在 allPrograms 链表中，等待父进程调用 wait 回收。
    }

    ListItem *item = readyPrograms.front();
    PCB *next = ListItem2PCB(item, tagInGeneralList);
    PCB *cur = running;
    next->status = ProgramStatus::RUNNING;
    running = next;
    readyPrograms.pop_front();

    //printf("schedule: %x %x\n", cur, next);

    activateProgramPage(next);

    asm_switch_thread(cur, next);

    interruptManager.setInterruptStatus(status);
}

void program_exit()
{
    PCB *thread = programManager.running;
    thread->status = ProgramStatus::DEAD;

    if (thread->pid)
    {
        programManager.schedule();
    }
    else
    {
        interruptManager.disableInterrupt();
        printf("halt\n");
        asm_halt();
    }
}

PCB *ProgramManager::allocatePCB()
{
    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        if (!PCB_SET_STATUS[i])
        {
            PCB_SET_STATUS[i] = true;
            return (PCB *)((int)PCB_SET + PCB_SIZE * i);
        }
    }

    return nullptr;
}

void ProgramManager::releasePCB(PCB *program)
{
    int index = ((int)program - (int)PCB_SET) / PCB_SIZE;
    PCB_SET_STATUS[index] = false;
    this->allPrograms.erase(&(program->tagInAllList));
}

void ProgramManager::MESA_WakeUp(PCB *program)
{
    program->status = ProgramStatus::READY;
    //printf("wake up program, pid: %d\n", program->pid);
    readyPrograms.push_front(&(program->tagInGeneralList));
}

void ProgramManager::initializeTSS()
{

    int size = sizeof(TSS);
    int address = (int)&tss;

    memset((char *)address, 0, size);
    tss.ss0 = STACK_SELECTOR; // 内核态堆栈段选择子

    int low, high, limit;

    limit = size - 1;
    low = (address << 16) | (limit & 0xff);
    // DPL = 0
    high = (address & 0xff000000) | ((address & 0x00ff0000) >> 16) | ((limit & 0xff00) << 16) | 0x00008900;

    int selector = asm_add_global_descriptor(low, high);
    // RPL = 0
    asm_ltr(selector << 3);
    tss.ioMap = address + size;
}

int ProgramManager::executeProcess(const char *filename, int priority)
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 在线程创建的基础上初步创建进程的PCB
    int pid = executeThread((ThreadFunction)load_process,
                            (void *)filename, filename, priority);
    if (pid == -1)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 找到刚刚创建的PCB
    PCB *process = ListItem2PCB(allPrograms.back(), tagInAllList);

    // 创建进程的页目录表
    process->pageDirectoryAddress = createProcessPageDirectory();
    //printf("%x\n", process->pageDirectoryAddress);

    if (!process->pageDirectoryAddress)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    // 创建进程的虚拟地址池
    bool res = createUserVirtualPool(process);

    if (!res)
    {
        process->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    interruptManager.setInterruptStatus(status);

    return pid;
}

int ProgramManager::createProcessPageDirectory()
{
    // 从内核地址池中分配一页存储用户进程的页目录表
    int vaddr = memoryManager.allocatePages(AddressPoolType::KERNEL, 1);
    if (!vaddr)
    {
        //printf("can not create page from kernel\n");
        return 0;
    }

    memset((char *)vaddr, 0, PAGE_SIZE);

    // 复制内核目录项到虚拟地址的高1GB
    int *src = (int *)(0xfffff000 + 0x300 * 4);
    int *dst = (int *)(vaddr + 0x300 * 4);
    for (int i = 0; i < 256; ++i)
    {
        dst[i] = src[i];
    }

    // 用户进程页目录表的最后一项指向用户进程页目录表本身
    ((int *)vaddr)[1023] = memoryManager.vaddr2paddr(vaddr) | 0x7;

    return vaddr;
}

bool ProgramManager::createUserVirtualPool(PCB *process)
{
    int sourcesCount = (0xc0000000 - USER_VADDR_START) / PAGE_SIZE;
    int bitmapLength = ceil(sourcesCount, 8);

    // 计算位图所占的页数
    int pagesCount = ceil(bitmapLength, PAGE_SIZE);

    int start = memoryManager.allocatePages(AddressPoolType::KERNEL, pagesCount);
    //printf("%x %d\n", start, pagesCount);

    if (!start)
    {
        return false;
    }

    memset((char *)start, 0, PAGE_SIZE * pagesCount);
    (process->userVirtual).initialize((char *)start, sourcesCount, USER_VADDR_START);

    return true;
}

void load_process(const char *filename)
{
    interruptManager.disableInterrupt();

    PCB *process = programManager.running;
    ProcessStartStack *interruptStack =
        (ProcessStartStack *)((int)process + PAGE_SIZE - sizeof(ProcessStartStack));

    interruptStack->edi = 0;
    interruptStack->esi = 0;
    interruptStack->ebp = 0;
    interruptStack->esp_dummy = 0;
    interruptStack->ebx = 0;
    interruptStack->edx = 0;
    interruptStack->ecx = 0;
    interruptStack->eax = 0;
    interruptStack->gs = 0;

    interruptStack->fs = programManager.USER_DATA_SELECTOR;
    interruptStack->es = programManager.USER_DATA_SELECTOR;
    interruptStack->ds = programManager.USER_DATA_SELECTOR;

    interruptStack->eip = (int)filename;
    interruptStack->cs = programManager.USER_CODE_SELECTOR;   // 用户模式平坦模式
    interruptStack->eflags = (0 << 12) | (1 << 9) | (1 << 1); // IOPL, IF = 1 开中断, MBS = 1 默认

    interruptStack->esp = memoryManager.allocatePages(AddressPoolType::USER, 1);
    if (interruptStack->esp == 0)
    {
        printf("can not build process!\n");
        process->status = ProgramStatus::DEAD;
        asm_halt();
    }
    interruptStack->esp += PAGE_SIZE;

    // 设置进程返回地址
    int *userStack = (int *)interruptStack->esp;
    userStack -= 3;
    userStack[0] = (int)exit;
    userStack[1] = 0;
    userStack[2] = 0;

    interruptStack->esp = (int)userStack;

    interruptStack->ss = programManager.USER_STACK_SELECTOR;

    asm_start_process((int)interruptStack);
}

void ProgramManager::activateProgramPage(PCB *program)
{
    int paddr = PAGE_DIRECTORY;

    if (program->pageDirectoryAddress)
    {
        tss.esp0 = (int)program + PAGE_SIZE;
        paddr = memoryManager.vaddr2paddr(program->pageDirectoryAddress);
    }

    asm_update_cr3(paddr);
}

int ProgramManager::fork()
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 禁止内核线程调用
    PCB *parent = this->running;
    if (!parent->pageDirectoryAddress)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    int pid = executeProcess("", 0);
    if (pid == -1)
    {
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    PCB *child = ListItem2PCB(this->allPrograms.back(), tagInAllList);
    bool flag = copyProcess(parent, child);

    if (!flag)
    {
        child->status = ProgramStatus::DEAD;
        interruptManager.setInterruptStatus(status);
        return -1;
    }

    interruptManager.setInterruptStatus(status);
    return pid;
}

bool ProgramManager::copyProcess(PCB *parent, PCB *child)
{
    // 复制PCB
    ProcessStartStack *childpps = (ProcessStartStack *)((int)child + PAGE_SIZE - sizeof(ProcessStartStack));
    ProcessStartStack *parentpps = (ProcessStartStack *)((int)parent + PAGE_SIZE - sizeof(ProcessStartStack));
    memcpy(parentpps, childpps, sizeof(ProcessStartStack));
    childpps->eax = 0;

    child->stack = (int *)childpps - 7;
    child->stack[0] = 0;
    child->stack[1] = 0;
    child->stack[2] = 0;
    child->stack[3] = 0;
    child->stack[4] = (int)asm_start_process;
    child->stack[5] = 0;             // asm_start_process 返回地址
    child->stack[6] = (int)childpps; // asm_start_process 参数

    child->status = ProgramStatus::READY;
    child->parentPid = parent->pid;
    child->priority = parent->priority;
    child->ticks = parent->ticks;
    child->ticksPassedBy = parent->ticksPassedBy;
    strcpy(parent->name, child->name);

    // 复制用户虚拟地址池
    int bitmapLength = parent->userVirtual.resources.length;
    int bitmapBytes = ceil(bitmapLength, 8);
    memcpy(parent->userVirtual.resources.bitmap, child->userVirtual.resources.bitmap, bitmapBytes);

    char *buffer = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 1);
    if (!buffer)
    {
        child->status = ProgramStatus::DEAD;
        return false;
    }

    // 子进程页目录表地址
    int childPageDirPaddr = memoryManager.vaddr2paddr(child->pageDirectoryAddress);
    // 父进程页目录表地址
    int parentPageDirPaddr = memoryManager.vaddr2paddr(parent->pageDirectoryAddress);
    // 子进程页目录表指针(虚拟地址)
    int *childPageDir = (int *)child->pageDirectoryAddress;
    // 父进程页目录表指针(虚拟地址)
    int *parentPageDir = (int *)parent->pageDirectoryAddress;

    //printf("%x %x\n", parent->pageDirectoryAddress, child->pageDirectoryAddress);

    memset((void *)child->pageDirectoryAddress, 0, 768 * 4);

    for (int i = 0; i < 768; ++i)
    {
        // 无对应页表
        if (!(parentPageDir[i] & 0x1))
        {
            continue;
        }

        // 从用户物理地址池中分配一页，作为子进程的页目录项指向的页表
        int paddr = memoryManager.allocatePhysicalPages(AddressPoolType::USER, 1);
        if (!paddr)
        {
            child->status = ProgramStatus::DEAD;
            return false;
        }
        // 页目录项
        int pde = parentPageDir[i];
        // 构造页表的起始虚拟地址
        int *pageTableVaddr = (int *)(0xffc00000 + (i << 12));

        asm_update_cr3(childPageDirPaddr); // 进入子进程虚拟地址空间

        childPageDir[i] = (pde & 0x00000fff) | paddr;
        memset(pageTableVaddr, 0, PAGE_SIZE);

        asm_update_cr3(parentPageDirPaddr); // 回到父进程虚拟地址空间
    }

    for (int i = 0; i < 768; ++i)
    {
        // 无对应页表
        if (!(parentPageDir[i] & 0x1))
        {
            continue;
        }

        // 计算页表的虚拟地址
        int *pageTableVaddr = (int *)(0xffc00000 + (i << 12));

        // 复制物理页
        for (int j = 0; j < 1024; ++j)
        {
            // 无对应物理页
            if (!(pageTableVaddr[j] & 0x1))
            {
                continue;
            }

            // 从用户物理地址池中分配一页，作为子进程的页表项指向的物理页
            int paddr = memoryManager.allocatePhysicalPages(AddressPoolType::USER, 1);
            if (!paddr)
            {
                child->status = ProgramStatus::DEAD;
                return false;
            }

            // 构造物理页的起始虚拟地址
            void *pageVaddr = (void *)((i << 22) + (j << 12));
            memcpy(pageVaddr, buffer, PAGE_SIZE);
            // 页表项
            int pte = pageTableVaddr[j];

            asm_update_cr3(childPageDirPaddr); // 进入子进程虚拟地址空间

            pageTableVaddr[j] = (pte & 0x00000fff) | paddr;
            memcpy(buffer, pageVaddr, PAGE_SIZE);

            asm_update_cr3(parentPageDirPaddr); // 回到父进程虚拟地址空间
        }
    }

    // 归还从内核分配的页表
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)buffer, 1);
    return true;
}

void ProgramManager::exit(int ret)
{
    interruptManager.disableInterrupt();

    PCB *program = this->running;
    program->retValue = ret;
    program->status = ProgramStatus::DEAD;

    // === 关键：父进程退出时，立即处理其所有子进程 ===
    // 在释放自身内存资源之前，遍历 allPrograms 链表：
    // - 已是 DEAD 的子进程（僵尸进程）→ 直接 releasePCB 回收
    // - 仍存活的子进程（将成为孤儿）→ 将其 parentPid 改为 REAPER_PID(0) 托管给 reaper
    // 这样保证不会有僵尸进程残留，也不会有孤儿进程失去回收者。
    adoptOrReleaseChildren(program);

    int *pageDir, *page;
    int paddr;

    // === 释放进程自身的虚拟地址空间资源 ===
    // 只有当 pageDirectoryAddress != 0 时才是用户进程（线程跳过）
    if (program->pageDirectoryAddress)
    {
        pageDir = (int *)program->pageDirectoryAddress;
        for (int i = 0; i < 768; ++i)
        {
            if (!(pageDir[i] & 0x1))
            {
                continue;
            }

            page = (int *)(0xffc00000 + (i << 12));

            for (int j = 0; j < 1024; ++j)
            {
                if (!(page[j] & 0x1))
                {
                    continue;
                }

                paddr = memoryManager.vaddr2paddr((i << 22) + (j << 12));
                memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
            }

            paddr = memoryManager.vaddr2paddr((int)page);
            memoryManager.releasePhysicalPages(AddressPoolType::USER, paddr, 1);
        }

        memoryManager.releasePages(AddressPoolType::KERNEL, (int)pageDir, 1);

        int bitmapBytes = ceil(program->userVirtual.resources.length, 8);
        int bitmapPages = ceil(bitmapBytes, PAGE_SIZE);

        memoryManager.releasePages(AddressPoolType::KERNEL, (int)program->userVirtual.resources.bitmap, bitmapPages);
    }

    // 调用 schedule() 切换到下一个就绪进程。
    // 当前进程的 PCB 在 schedule() 中根据 pageDirectoryAddress/parentPid 决定是否回收：
    // - 线程或 parentPid==REAPER_PID 的孤儿进程 → 直接 releasePCB
    // - 普通子进程 → 保留，等待父进程 wait
    schedule();
}

// === 父进程退出时处理子进程：回收僵尸，托管孤儿 ===
// 该函数在 exit() 中被调用，遍历 allPrograms 链表，
// 找到所有 parentPid == 当前进程 pid 的子进程，分两种情况处理：
//
// 情况1 — 僵尸进程（child->status == DEAD）：
//   父进程退出后再也没有人会 wait 这个子进程，
//   其 PCB 将永远残留成为僵尸。此处直接 releasePCB 回收。
//
// 情况2 — 孤儿进程（child->status != DEAD）：
//   父进程退出后子进程还活着，无人回收其退出后的 PCB。
//   将子进程的 parentPid 改为 REAPER_PID(0)，
//   此后子进程退出时，schedule() 检测到 parentPid==0 会自动回收。
void ProgramManager::adoptOrReleaseChildren(PCB *parent)
{
    ListItem *item = this->allPrograms.head.next;

    while (item)
    {
        ListItem *next = item->next;  // 先保存 next，防止 releasePCB 后 item 失效
        PCB *child = ListItem2PCB(item, tagInAllList);

        if (child->parentPid == parent->pid)
        {
            if (child->status == ProgramStatus::DEAD)
            {
                // 僵尸进程：父进程退出后无人回收，直接释放 PCB
                releasePCB(child);
            }
            else
            {
                // 孤儿进程：托管给 REAPER_PID(0)，后续退出时由 schedule 自动回收
                child->parentPid = REAPER_PID;
            }
        }

        item = next;
    }
}

int ProgramManager::wait(int *retval)
{
    PCB *child;
    ListItem *item;
    bool interrupt, flag;

    while (true)
    {
        interrupt = interruptManager.getInterruptStatus();
        interruptManager.disableInterrupt();

        item = this->allPrograms.head.next;

        // 查找子进程
        flag = true;
        while (item)
        {
            child = ListItem2PCB(item, tagInAllList);
            if (child->parentPid == this->running->pid)
            {
                flag = false;
                if (child->status == ProgramStatus::DEAD)
                {
                    break;
                }
            }
            item = item->next;
        }

        if (item) // 找到一个可返回的子进程
        {
            if (retval)
            {
                *retval = child->retValue;
            }

            int pid = child->pid;
            releasePCB(child);
            interruptManager.setInterruptStatus(interrupt);
            return pid;
        }
        else 
        {
            if (flag) // 子进程已经返回
            {
                
                interruptManager.setInterruptStatus(interrupt);
                return -1;
            }
            else // 存在子进程，但子进程的状态不是DEAD
            {
                interruptManager.setInterruptStatus(interrupt);
                schedule();
            }
        }
    }
}
