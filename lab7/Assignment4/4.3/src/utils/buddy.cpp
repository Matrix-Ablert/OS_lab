#include "buddy.h"
#include "stdio.h"
#include "stdlib.h"
#include "os_constant.h"

BuddyAllocator::BuddyAllocator()
    : startAddress(0), totalPages(0), allocatedPages(0),
      maxOrder(0), blockPoolIndex(0), allocRecordCount(0)
{
    for (int i = 0; i <= BUDDY_MAX_ORDER; ++i)
    {
        freeArea[i] = nullptr;
    }
    for (int i = 0; i < BUDDY_BLOCK_POOL; ++i)
    {
        blockPool[i].address = 0;
        blockPool[i].next = nullptr;
    }
    for (int i = 0; i < BUDDY_ALLOC_TRACK; ++i)
    {
        allocRecords[i].address = 0;
        allocRecords[i].order = 0;
    }
}

void BuddyAllocator::initialize(int startAddr, int totalPageCount)
{
    startAddress = startAddr;
    totalPages = totalPageCount;
    allocatedPages = 0;
    blockPoolIndex = 0;
    allocRecordCount = 0;

    // 计算最大阶数：最大的 2^order <= totalPageCount
    maxOrder = 0;
    int pages = totalPageCount;
    while (pages > 1)
    {
        pages >>= 1;
        ++maxOrder;
    }

    // 将全部空闲页作为一个大块加入最高阶的 free list
    addBlock(startAddress, maxOrder);

    printf("[Buddy] Initialized: base=0x%x, pages=%d, maxOrder=%d (block size=%d pages)\n",
           startAddr, totalPageCount, maxOrder, 1 << maxOrder);
}

int BuddyAllocator::getOrder(int pageCount) const
{
    int order = 0;
    int size = 1;
    while (size < pageCount)
    {
        size <<= 1;
        ++order;
    }
    return order;
}

int BuddyAllocator::buddyOf(int addr, int order) const
{
    // 伙伴地址 = addr XOR (块大小)
    int blockSize = (1 << order) * PAGE_SIZE;
    return addr ^ blockSize;
}

BuddyAllocator::BlockNode *BuddyAllocator::allocBlockNode()
{
    if (blockPoolIndex >= BUDDY_BLOCK_POOL)
    {
        printf("[Buddy] WARNING: BlockNode pool exhausted!\n");
        return nullptr;
    }
    return &blockPool[blockPoolIndex++];
}

void BuddyAllocator::freeBlockNode(BlockNode * /*node*/)
{
    // 节点在 BSS 池中，不需要释放
}

void BuddyAllocator::addBlock(int addr, int order)
{
    if (order < 0 || order > maxOrder)
        return;

    BlockNode *node = allocBlockNode();
    if (!node)
        return;

    node->address = addr;
    node->next = freeArea[order];
    freeArea[order] = node;
}

int BuddyAllocator::popBlock(int order)
{
    if (order < 0 || order > maxOrder)
        return 0;
    if (!freeArea[order])
        return 0;

    BlockNode *node = freeArea[order];
    int addr = node->address;
    freeArea[order] = node->next;
    freeBlockNode(node);
    return addr;
}

bool BuddyAllocator::removeBlock(int addr, int order)
{
    if (order < 0 || order > maxOrder)
        return false;

    BlockNode *prev = nullptr;
    BlockNode *curr = freeArea[order];

    while (curr)
    {
        if (curr->address == addr)
        {
            // 从链表中移除
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                freeArea[order] = curr->next;
            }
            freeBlockNode(curr);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

void BuddyAllocator::trackAlloc(int addr, int order)
{
    if (allocRecordCount < BUDDY_ALLOC_TRACK)
    {
        allocRecords[allocRecordCount].address = addr;
        allocRecords[allocRecordCount].order = order;
        ++allocRecordCount;
    }
    else
    {
        printf("[Buddy] WARNING: AllocRecord table full!\n");
    }
}

int BuddyAllocator::findAllocRecord(int addr)
{
    for (int i = 0; i < allocRecordCount; ++i)
    {
        if (allocRecords[i].address == addr)
        {
            int order = allocRecords[i].order;

            // 移除记录
            allocRecords[i] = allocRecords[allocRecordCount - 1];
            --allocRecordCount;

            return order;
        }
    }
    return -1; // 未找到
}

int BuddyAllocator::allocate(int pageCount)
{
    int targetOrder = getOrder(pageCount);

    // 从目标阶开始查找可用块
    int order = targetOrder;
    while (order <= maxOrder && !freeArea[order])
    {
        ++order;
    }

    if (order > maxOrder)
    {
        printf("[Buddy] FAILED: no free block for %d pages (order %d)\n",
               pageCount, targetOrder);
        return 0;
    }

    // 取出块并分割到目标阶
    int addr = popBlock(order);
    while (order > targetOrder)
    {
        --order;
        int halfSize = (1 << order) * PAGE_SIZE;
        // 分割：保留左半块继续分割，右半块加入 free list
        addBlock(addr + halfSize, order);
        // addr 保持不变（左半块）
    }

    int actualPages = 1 << targetOrder;
    allocatedPages += actualPages;
    trackAlloc(addr, targetOrder);

    return addr;
}

void BuddyAllocator::release(int addr)
{
    if (addr < startAddress || addr >= startAddress + totalPages * PAGE_SIZE)
    {
        printf("[Buddy] ERROR: invalid release address 0x%x\n", addr);
        return;
    }

    // 查找分配记录获取阶数
    int order = findAllocRecord(addr);
    if (order < 0)
    {
        printf("[Buddy] WARNING: no alloc record for addr 0x%x, assuming order 0\n", addr);
        order = 0;
    }

    allocatedPages -= (1 << order);

    // 释放并尝试逐级合并
    int currentAddr = addr;
    int currentOrder = order;

    while (currentOrder < maxOrder)
    {
        int buddyAddr = buddyOf(currentAddr, currentOrder);

        // 检查伙伴是否在 free list 中
        if (!removeBlock(buddyAddr, currentOrder))
        {
            break; // 伙伴未空闲，停止合并
        }

        // 伙伴空闲，合并
        if (currentAddr > buddyAddr)
        {
            currentAddr = buddyAddr;
        }
        ++currentOrder;
    }

    // 将（可能合并后的）块加入对应阶的 free list
    addBlock(currentAddr, currentOrder);
}

void BuddyAllocator::printInfo()
{
    int freePageCount = 0;
    printf("===== Buddy Allocator Status =====\n");
    printf("Base: 0x%x, Total: %d pages (%d KB)\n",
           startAddress, totalPages, totalPages * 4);
    printf("Allocated: %d pages (%d KB)\n", allocatedPages, allocatedPages * 4);

    printf("Free block lists:\n");
    for (int i = 0; i <= maxOrder; ++i)
    {
        int count = 0;
        BlockNode *curr = freeArea[i];
        while (curr)
        {
            ++count;
            freePageCount += (1 << i);
            curr = curr->next;
        }
        if (count > 0)
        {
            printf("  Order %d (%d pages): %d block(s), first=0x%x\n",
                   i, 1 << i, count,
                   freeArea[i] ? freeArea[i]->address : 0);
        }
    }
    printf("Free total: %d pages (%d KB)\n", freePageCount, freePageCount * 4);
    printf("==================================\n");
}
