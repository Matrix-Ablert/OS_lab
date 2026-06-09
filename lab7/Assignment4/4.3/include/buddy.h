#ifndef BUDDY_H
#define BUDDY_H

#include "os_type.h"

// Buddy System 常量
#define BUDDY_MAX_ORDER 12     // 最大阶：2^12 = 4096 页 = 16MB
#define BUDDY_BLOCK_POOL 2048  // 预分配的 block 描述符数量
#define BUDDY_ALLOC_TRACK 1024 // 最多追踪 1024 个分配块

// Buddy System 物理页分配器
// 将物理内存按 2^n 大小分组管理，有效减少外部碎片
class BuddyAllocator
{
public:
    // 空闲块链表节点
    struct BlockNode
    {
        int address;        // 物理页地址（页对齐）
        BlockNode *next;    // 链表下一个节点
    };

private:
    int startAddress;          // 管理的物理内存起始地址
    int totalPages;            // 管理的物理页总数
    int allocatedPages;        // 已分配的物理页数
    int maxOrder;              // 实际最大阶数

    // 空闲块链表数组：freeArea[order] 指向该阶空闲块链表头
    BlockNode *freeArea[BUDDY_MAX_ORDER + 1];

    // 预分配的 BlockNode 池
    BlockNode blockPool[BUDDY_BLOCK_POOL];
    int blockPoolIndex;

    // 已分配块的追踪表（记录地址→阶数的映射，用于释放）
    struct AllocRecord {
        int address;
        int order;
    };
    AllocRecord allocRecords[BUDDY_ALLOC_TRACK];
    int allocRecordCount;

public:
    BuddyAllocator();

    // 初始化 Buddy System，管理 [startAddr, startAddr + totalPageCount * PAGE_SIZE) 范围内的物理页
    void initialize(int startAddr, int totalPageCount);

    // 分配 count 个连续物理页，返回起始物理地址（失败返回 0）
    int allocate(int pageCount);

    // 释放从 addr 开始的物理块（addr 必须由 allocate 返回）
    void release(int addr);

    // 获取统计信息
    int getAllocatedPages() const { return allocatedPages; }
    int getTotalPages() const { return totalPages; }
    int getFreePages() const { return totalPages - allocatedPages; }

    // 打印 Buddy System 状态
    void printInfo();

private:
    // 计算所需的最小阶数（向上取整到 2^n）
    int getOrder(int pageCount) const;

    // 计算地址 addr 在 order 阶下的伙伴地址
    int buddyOf(int addr, int order) const;

    // 将一个空闲块添加到对应阶的 free list
    void addBlock(int addr, int order);

    // 从指定阶的 free list 中取出一个块
    int popBlock(int order);

    // 从指定阶的 free list 中移除指定地址的块，成功返回 true
    bool removeBlock(int addr, int order);

    // 从 BSS 池中分配一个 BlockNode
    BlockNode *allocBlockNode();

    // 释放 BlockNode 回池
    void freeBlockNode(BlockNode *node);

    // 追踪一个分配块
    void trackAlloc(int addr, int order);

    // 查找分配块记录，返回 order
    int findAllocRecord(int addr);
};

#endif
