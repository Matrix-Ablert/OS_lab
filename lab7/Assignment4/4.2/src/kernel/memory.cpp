#include "memory.h"
#include "os_constant.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "stdio.h"
#include "program.h"
#include "os_modules.h"

MemoryManager::MemoryManager()
{
    initialize();
}

void MemoryManager::initialize()
{
    this->totalMemory = 0;
    this->totalMemory = getTotalMemory();

    // 预留的内存
    int usedMemory = 256 * PAGE_SIZE + 0x100000;
    if (this->totalMemory < usedMemory)
    {
        printf("memory is too small, halt.\n");
        asm_halt();
    }
    // 剩余的空闲的内存
    int freeMemory = this->totalMemory - usedMemory;

    int freePages = freeMemory / PAGE_SIZE;
    int kernelPages = freePages / 2;
    int userPages = freePages - kernelPages;

    int kernelPhysicalStartAddress = usedMemory;
    int userPhysicalStartAddress = usedMemory + kernelPages * PAGE_SIZE;

    int kernelPhysicalBitMapStart = BITMAP_START_ADDRESS;
    int userPhysicalBitMapStart = kernelPhysicalBitMapStart + ceil(kernelPages, 8);
    int kernelVirtualBitMapStart = userPhysicalBitMapStart + ceil(userPages, 8);

    kernelPhysical.initialize(
        (char *)kernelPhysicalBitMapStart,
        kernelPages,
        kernelPhysicalStartAddress);

    userPhysical.initialize(
        (char *)userPhysicalBitMapStart,
        userPages,
        userPhysicalStartAddress);

    kernelVirtual.initialize(
        (char *)kernelVirtualBitMapStart,
        kernelPages,
        KERNEL_VIRTUAL_START);

    printf("total memory: %d bytes ( %d MB )\n",
           this->totalMemory,
           this->totalMemory / 1024 / 1024);

    printf("kernel pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bitmap start address: 0x%x\n",
           kernelPhysicalStartAddress,
           kernelPages, kernelPages * PAGE_SIZE / 1024 / 1024,
           kernelPhysicalBitMapStart);

    printf("user pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bit map start address: 0x%x\n",
           userPhysicalStartAddress,
           userPages, userPages * PAGE_SIZE / 1024 / 1024,
           userPhysicalBitMapStart);

    printf("kernel virtual pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d  ( %d MB ) \n"
           "    bit map start address: 0x%x\n",
           KERNEL_VIRTUAL_START,
           userPages, kernelPages * PAGE_SIZE / 1024 / 1024,
           kernelVirtualBitMapStart);
}

int MemoryManager::allocatePhysicalPages(enum AddressPoolType type, const int count)
{
    int start = -1;

    if (type == AddressPoolType::KERNEL)
    {
        start = kernelPhysical.allocate(count);
    }
    else if (type == AddressPoolType::USER)
    {
        start = userPhysical.allocate(count);
    }

    return (start == -1) ? 0 : start;
}

void MemoryManager::releasePhysicalPages(enum AddressPoolType type, const int paddr, const int count)
{
    if (type == AddressPoolType::KERNEL)
    {
        kernelPhysical.release(paddr, count);
    }
    else if (type == AddressPoolType::USER)
    {

        userPhysical.release(paddr, count);
    }
}

int MemoryManager::getTotalMemory()
{

    if (!this->totalMemory)
    {
        int memory = *((int *)MEMORY_SIZE_ADDRESS);
        // ax寄存器保存的内容
        int low = memory & 0xffff;
        // bx寄存器保存的内容
        int high = (memory >> 16) & 0xffff;

        this->totalMemory = low * 1024 + high * 64 * 1024;
    }

    return this->totalMemory;
}

void MemoryManager::openPageMechanism()
{
    // 页目录表指针
    int *directory = (int *)PAGE_DIRECTORY;
    //线性地址0~4MB对应的页表
    int *page = (int *)(PAGE_DIRECTORY + PAGE_SIZE);

    // 初始化页目录表
    memset(directory, 0, PAGE_SIZE);
    // 初始化线性地址0~4MB对应的页表
    memset(page, 0, PAGE_SIZE);

    int address = 0;
    // 将线性地址0~1MB恒等映射到物理地址0~1MB
    for (int i = 0; i < 256; ++i)
    {
        // U/S = 1, R/W = 1, P = 1
        page[i] = address | 0x7;
        address += PAGE_SIZE;
    }

    // 初始化页目录项

    // 0~1MB
    directory[0] = ((int)page) | 0x07;
    // 3GB的内核空间
    directory[768] = directory[0];
    // 最后一个页目录项指向页目录表
    directory[1023] = ((int)directory) | 0x7;

    // 初始化cr3，cr0，开启分页机制
    asm_init_page_reg(directory);

    printf("open page mechanism\n");
}

int MemoryManager::allocatePages(enum AddressPoolType type, const int count)
{
    // 第一步：从虚拟地址池中分配若干虚拟页
    int virtualAddress = allocateVirtualPages(type, count);
    if (!virtualAddress)
    {
        return 0;
    }

    bool flag;
    int physicalPageAddress;
    int vaddress = virtualAddress;

    // 依次为每一个虚拟页指定物理页
    for (int i = 0; i < count; ++i, vaddress += PAGE_SIZE)
    {
        flag = false;
        // 第二步：从物理地址池中分配一个物理页
        physicalPageAddress = allocatePhysicalPages(type, 1);
        if (physicalPageAddress)
        {
            //printf("allocate physical page 0x%x\n", physicalPageAddress);

            // 第三步：为虚拟页建立页目录项和页表项，使虚拟页内的地址经过分页机制变换到物理页内。
            flag = connectPhysicalVirtualPage(vaddress, physicalPageAddress);
        }
        else
        {
            flag = false;
        }

        // 分配失败，释放前面已经分配的虚拟页和物理页表
        if (!flag)
        {
            // 前i个页表已经指定了物理页
            releasePages(type, virtualAddress, i);
            // 剩余的页表未指定物理页
            releaseVirtualPages(type, virtualAddress + i * PAGE_SIZE, count - i);
            return 0;
        }
    }

    return virtualAddress;
}

int MemoryManager::allocateVirtualPages(enum AddressPoolType type, const int count)
{
    int start = -1;

    if (type == AddressPoolType::KERNEL)
    {
        start = kernelVirtual.allocate(count);
    }

    return (start == -1) ? 0 : start;
}

bool MemoryManager::connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress)
{
    // 计算虚拟地址对应的页目录项和页表项
    int *pde = (int *)toPDE(virtualAddress);
    int *pte = (int *)toPTE(virtualAddress);

    // 页目录项无对应的页表，先分配一个页表
    if(!(*pde & 0x00000001)) 
    {
        // 从内核物理地址空间中分配一个页表
        int page = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
        if (!page)
            return false;

        // 使页目录项指向页表
        *pde = page | 0x7;
        // 初始化页表
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }

    // 使页表项指向物理页
    *pte = physicalPageAddress | 0x7;

    return true;
}

int MemoryManager::toPDE(const int virtualAddress)
{
    return (0xfffff000 + (((virtualAddress & 0xffc00000) >> 22) * 4));
}

int MemoryManager::toPTE(const int virtualAddress)
{
    return (0xffc00000 + ((virtualAddress & 0xffc00000) >> 10) + (((virtualAddress & 0x003ff000) >> 12) * 4));
}

void MemoryManager::releasePages(enum AddressPoolType type, const int virtualAddress, const int count)
{
    int vaddr = virtualAddress;
    int *pte;
    for (int i = 0; i < count; ++i, vaddr += PAGE_SIZE)
    {
        // 第一步，对每一个虚拟页，释放为其分配的物理页
        releasePhysicalPages(type, vaddr2paddr(vaddr), 1);

        // 设置页表项为不存在，防止释放后被再次使用
        pte = (int *)toPTE(vaddr);
        *pte = 0;
    }

    // 第二步，释放虚拟页
    releaseVirtualPages(type, virtualAddress, count);
}

int MemoryManager::vaddr2paddr(int vaddr)
{
    int *pte = (int *)toPTE(vaddr);
    int page = (*pte) & 0xfffff000;
    int offset = vaddr & 0xfff;
    return (page + offset);
}

void MemoryManager::releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count)
{
    if (type == AddressPoolType::KERNEL)
    {
        kernelVirtual.release(vaddr, count);
    }
}

void MemoryManager::printMemoryStatus()
{
    printf("\n===== Memory Status =====\n");

    // 1. 物理内存总量
    printf("Total Physical Memory: %d KB (%d MB)\n",
           totalMemory / 1024, totalMemory / 1024 / 1024);

    // 2. 内核物理地址池
    int kp_total = kernelPhysical.resources.size();
    int kp_used = kernelPhysical.resources.countAllocated();
    int kp_free = kp_total - kp_used;
    printf("Kernel Physical Pool: %d pages total, %d used (%d%%), %d free\n",
           kp_total, kp_used, kp_total ? (kp_used * 100 / kp_total) : 0, kp_free);

    // 3. 用户物理地址池
    int up_total = userPhysical.resources.size();
    int up_used = userPhysical.resources.countAllocated();
    int up_free = up_total - up_used;
    printf("User Physical Pool: %d pages total, %d used (%d%%), %d free\n",
           up_total, up_used, up_total ? (up_used * 100 / up_total) : 0, up_free);

    // 4. 内核虚拟地址池
    int kv_total = kernelVirtual.resources.size();
    int kv_used = kernelVirtual.resources.countAllocated();
    int kv_free = kv_total - kv_used;
    printf("Kernel Virtual Pool: %d pages total, %d used (%d%%), %d free\n",
           kv_total, kv_used, kv_total ? (kv_used * 100 / kv_total) : 0, kv_free);

    // 5. 统计页目录项和页表项数量
    int pde_count = 0;
    int pte_count = 0;

    // 页目录表位于虚拟地址 0xFFFFF000（通过 PDE[1023] 自映射）
    int *pde_table = (int *)0xFFFFF000;

    for (int i = 0; i < 1024; ++i)
    {
        if (pde_table[i] & 0x1)  // P 位为 1
        {
            ++pde_count;
            // 该页目录项对应的页表位于 0xFFC00000 + (i << 12)
            int *pte_table = (int *)(0xFFC00000 + (i << 12));
            for (int j = 0; j < 1024; ++j)
            {
                if (pte_table[j] & 0x1)  // P 位为 1
                {
                    ++pte_count;
                }
            }
        }
    }

    printf("Page Directory Entries (P=1): %d\n", pde_count);
    printf("Page Table Entries (P=1): %d\n", pte_count);

    printf("==========================\n\n");
}