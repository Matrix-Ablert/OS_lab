#ifndef BITMAP_H
#define BITMAP_H

#include "os_type.h"

// 分配策略枚举
enum AllocationStrategy
{
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT,
    NEXT_FIT
};

class BitMap
{
public:
    // 被管理的资源个数，bitmap的总位数
    int length;
    // bitmap的起始地址
    char *bitmap;
public:
    // 初始化
    BitMap();
    // 设置BitMap，bitmap=起始地址，length=总位数(即被管理的资源个数)
    void initialize(char *bitmap, const int length);
    // 获取第index个资源的状态，true=allocated，false=free
    bool get(const int index) const;
    // 设置第index个资源的状态，true=allocated，false=free
    void set(const int index, const bool status);
    // 分配count个连续的资源，若没有则返回-1，否则返回分配的第1个资源单元序号
    int allocate(const int count);
    // 释放第index个资源开始的count个资源
    void release(const int index, const int count);
    // 返回Bitmap存储区域
    char *getBitmap();
    // 返回Bitmap的大小
    int size() const;

    // ===== 新增：策略切换与统计 =====
    // 设置分配策略
    void setStrategy(AllocationStrategy s);
    // 获取当前策略
    AllocationStrategy getStrategy() const;
    // 统计已分配资源数
    int getUsedCount() const;
    // 统计最大连续空闲块大小
    int getMaxFreeBlock() const;
    // 统计空闲碎片数量（不连续的空闲段个数）
    int getFreeFragmentCount() const;

private:
    // 禁止Bitmap之间的赋值
    BitMap(const BitMap &) {}
    void operator=(const BitMap&) {}

    // 当前分配策略
    AllocationStrategy strategy;
    // Next Fit 用：上次分配结束位置
    int lastIndex;

    // 四种分配算法实现
    int allocateFirstFit(const int count);
    int allocateBestFit(const int count);
    int allocateWorstFit(const int count);
    int allocateNextFit(const int count);
};

#endif