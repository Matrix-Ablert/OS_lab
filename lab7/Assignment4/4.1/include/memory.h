#ifndef MEMORY_H
#define MEMORY_H

#include "address_pool.h"

#define MAX_PHYSICAL_PAGES 48

enum AddressPoolType
{
    USER,
    KERNEL
};

class MemoryManager
{
public:
    // 可管理的内存容量
    int totalMemory;
    // 内核物理地址池
    AddressPool kernelPhysical;
    // 用户物理地址池
    AddressPool userPhysical;
    // 内核虚拟地址池
    AddressPool kernelVirtual;

    // 页面置换相关成员
    int maxPhysicalPages;        // 限制的物理页帧总数
    int physicalSlots[48];       // 每个slots存储的物理页地址（内联数组，避免外部指针冲突）
    int slotVA[48];              // 每个slots对应的虚拟地址
    int pageAccessBits[48];      // Clock算法的访问位
    int clockHand;               // Clock置换算法的指针
    int allocatedPhysicalCount;  // 当前已分配的数据页数

public:
    MemoryManager();

    // 初始化地址池
    void initialize();

    // 从type类型的物理地址池中分配count个连续的页
    // 成功，返回起始地址；失败，返回0
    int allocatePhysicalPages(enum AddressPoolType type, const int count);

    // 释放从paddr开始的count个物理页
    void releasePhysicalPages(enum AddressPoolType type, const int startAddress, const int count);

    // 获取内存总容量
    int getTotalMemory();

    // 开启分页机制
    void openPageMechanism();

    // 页内存分配
    int allocatePages(enum AddressPoolType type, const int count);

    // 虚拟页分配
    int allocateVirtualPages(enum AddressPoolType type, const int count);

    // 建立虚拟页到物理页的联系
    bool connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress);

    // 计算virtualAddress的页目录项的虚拟地址
    int toPDE(const int virtualAddress);

    // 计算virtualAddress的页表项的虚拟地址
    int toPTE(const int virtualAddress);

    // 页内存释放
    void releasePages(enum AddressPoolType type, const int virtualAddress, const int count);    

    // 找到虚拟地址对应的物理地址
    int vaddr2paddr(int vaddr);

    // 释放虚拟页
    void releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count);

    // ===== 页面置换接口 =====
    // 初始化页面置换子系统
    void initPageReplacement();
    // 使用Clock算法选择被淘汰的页，返回其在slots数组中的索引
    int selectVictimPage();
    // 标记虚拟地址vaddr对应的页为已访问
    void accessPage(int vaddr);
    // 执行页面置换，淘汰一个旧页并返回其物理地址
    int pageReplacement(int vaddr);
};

#endif