#include "bitmap.h"
#include "stdlib.h"
#include "stdio.h"

BitMap::BitMap()
{
    strategy = FIRST_FIT;
    lastIndex = 0;
}

void BitMap::initialize(char *bitmap, const int length)
{
    this->bitmap = bitmap;
    this->length = length;

    int bytes = ceil(length, 8);

    for (int i = 0; i < bytes; ++i)
    {
        bitmap[i] = 0;
    }

    lastIndex = 0;
}

bool BitMap::get(const int index) const
{
    int pos = index / 8;
    int offset = index % 8;

    return (bitmap[pos] & (1 << offset));
}

void BitMap::set(const int index, const bool status)
{
    int pos = index / 8;
    int offset = index % 8;

    // 清0
    bitmap[pos] = bitmap[pos] & (~(1 << offset));

    // 置1
    if (status)
    {
        bitmap[pos] = bitmap[pos] | (1 << offset);
    }
}

// ===== 策略切换 =====
void BitMap::setStrategy(AllocationStrategy s)
{
    strategy = s;
    lastIndex = 0;  // 切换策略时重置 Next Fit 指针
}

AllocationStrategy BitMap::getStrategy() const
{
    return strategy;
}

// ===== 分配入口：根据策略分发 =====
int BitMap::allocate(const int count)
{
    switch (strategy)
    {
    case FIRST_FIT:
        return allocateFirstFit(count);
    case BEST_FIT:
        return allocateBestFit(count);
    case WORST_FIT:
        return allocateWorstFit(count);
    case NEXT_FIT:
        return allocateNextFit(count);
    default:
        return allocateFirstFit(count);
    }
}

// ===== First Fit（首次适应）=====
// 从头扫描，取第一个足够大的连续空闲块
int BitMap::allocateFirstFit(const int count)
{
    if (count == 0)
        return -1;

    int index, empty, start;

    index = 0;
    while (index < length)
    {
        // 越过已经分配的资源
        while (index < length && get(index))
            ++index;

        // 不存在连续的count个资源
        if (index == length)
            return -1;

        // 检查是否存在从index开始的连续count个资源
        empty = 0;
        start = index;
        while ((index < length) && (!get(index)) && (empty < count))
        {
            ++empty;
            ++index;
        }

        // 存在连续的count个资源
        if (empty == count)
        {
            for (int i = 0; i < count; ++i)
            {
                set(start + i, true);
            }

            return start;
        }
    }

    return -1;
}

// ===== Best Fit（最佳适应）=====
// 扫描所有空闲块，选择最小的能满足需求的块
int BitMap::allocateBestFit(const int count)
{
    if (count == 0)
        return -1;

    int bestStart = -1;
    int bestSize = length + 1; // 初始化为比最大可能还大的值
    int index = 0;

    while (index < length)
    {
        // 越过已分配的资源
        while (index < length && get(index))
            ++index;

        if (index == length)
            break;

        // 测量整个空闲块的大小
        int start = index;
        int free = 0;
        while (index < length && !get(index))
        {
            ++free;
            ++index;
        }

        // 如果这个空闲块满足需求，且比当前最佳更小，则更新最佳
        if (free >= count && free < bestSize)
        {
            bestSize = free;
            bestStart = start;
        }
    }

    if (bestStart == -1)
        return -1;

    for (int i = 0; i < count; ++i)
    {
        set(bestStart + i, true);
    }

    return bestStart;
}

// ===== Worst Fit（最坏适应）=====
// 扫描所有空闲块，选择最大的块
int BitMap::allocateWorstFit(const int count)
{
    if (count == 0)
        return -1;

    int worstStart = -1;
    int worstSize = -1;
    int index = 0;

    while (index < length)
    {
        // 越过已分配的资源
        while (index < length && get(index))
            ++index;

        if (index == length)
            break;

        // 测量整个空闲块的大小
        int start = index;
        int free = 0;
        while (index < length && !get(index))
        {
            ++free;
            ++index;
        }

        // 如果这个空闲块满足需求，且比当前最大更大，则更新
        if (free >= count && free > worstSize)
        {
            worstSize = free;
            worstStart = start;
        }
    }

    if (worstStart == -1)
        return -1;

    for (int i = 0; i < count; ++i)
    {
        set(worstStart + i, true);
    }

    return worstStart;
}

// ===== Next Fit（循环首次适应）=====
// 从上次分配结束位置开始搜索，到头后回绕
int BitMap::allocateNextFit(const int count)
{
    if (count == 0)
        return -1;

    int i, empty, start;

    // 第一轮：从 lastIndex 扫描到末尾
    i = lastIndex;
    while (i < length)
    {
        // 越过已分配的资源
        while (i < length && get(i))
            ++i;

        if (i == length)
            break;

        // 检查是否存在从 i 开始的连续 count 个资源
        empty = 0;
        start = i;
        while ((i < length) && (!get(i)) && (empty < count))
        {
            ++empty;
            ++i;
        }

        if (empty == count)
        {
            for (int j = 0; j < count; ++j)
            {
                set(start + j, true);
            }
            lastIndex = start + count;
            return start;
        }
    }

    // 第二轮：从 0 扫描到 lastIndex - 1
    i = 0;
    while (i < lastIndex)
    {
        while (i < lastIndex && get(i))
            ++i;

        if (i >= lastIndex)
            break;

        empty = 0;
        start = i;
        while ((i < lastIndex) && (!get(i)) && (empty < count))
        {
            ++empty;
            ++i;
        }

        if (empty == count)
        {
            for (int j = 0; j < count; ++j)
            {
                set(start + j, true);
            }
            lastIndex = start + count;
            return start;
        }
    }

    return -1;
}

void BitMap::release(const int index, const int count)
{
    for (int i = 0; i < count; ++i)
    {
        set(index + i, false);
    }
}

char *BitMap::getBitmap()
{
    return (char *)bitmap;
}

int BitMap::size() const
{
    return length;
}

// ===== 统计函数 =====
int BitMap::getUsedCount() const
{
    int count = 0;
    for (int i = 0; i < length; ++i)
    {
        if (get(i))
            ++count;
    }
    return count;
}

int BitMap::getMaxFreeBlock() const
{
    int maxBlock = 0;
    int index = 0;

    while (index < length)
    {
        // 越过已分配的资源
        while (index < length && get(index))
            ++index;

        if (index == length)
            break;

        // 测量空闲块大小
        int free = 0;
        while (index < length && !get(index))
        {
            ++free;
            ++index;
        }

        if (free > maxBlock)
            maxBlock = free;
    }

    return maxBlock;
}

int BitMap::getFreeFragmentCount() const
{
    int fragments = 0;
    int index = 0;

    while (index < length)
    {
        // 越过已分配的资源
        while (index < length && get(index))
            ++index;

        if (index == length)
            break;

        // 找到一个空闲段
        ++fragments;

        // 跳过整个空闲段
        while (index < length && !get(index))
            ++index;
    }

    return fragments;
}