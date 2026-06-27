#ifndef SYNC_H
#define SYNC_H

#include "os_type.h"
#include "list.h"

class SpinLock
{
private:
    uint32 bolt;

public:
    SpinLock();
    void initialize();
    void lock();
    void unlock();
};

class Semaphore
{
private:
    uint32 counter;
    List waiting;
    SpinLock semLock;

public:
    Semaphore();
    void initialize(uint32 counter);
    void P();
    void V();
};

/**
 * 读者-写者锁（Reader-Writer Lock），采用"读者优先"策略。
 * ------------------------------------------------------------------
 * 设计思想：
 *   - 多个读者可以同时读取（共享访问）
 *   - 写者必须独占访问（与读者互斥、与写者互斥）
 *   - 读者优先：只要有读者在读，新来的读者可以插队进入，写者必须等待
 *
 * 读者优先的"饥饿"问题：
 *   当读者源源不断进入时，readCount 始终 > 0，wrtLock 永远不会释放，
 *   导致写者永远无法获取锁 → 写者饥饿（Starvation）。
 * ------------------------------------------------------------------
 */
class RWLock
{
private:
    Semaphore mutex;      // 保护 readCount 的互斥信号量（初始值 1）
    Semaphore wrtLock;    // 写者锁：控制写者的独占访问（初始值 1）
                          // 当 readCount > 0 时被读者持有，写者无法进入
    int readCount;        // 当前正在临界区内读取的读者数量

public:
    RWLock();
    void initialize();    // 初始化：mutex=1, wrtLock=1, readCount=0
    void readLock();      // 读者获取锁：首个读者同时获取 wrtLock
    void readUnlock();    // 读者释放锁：末个读者同时释放 wrtLock
    void writeLock();     // 写者获取锁：直接竞争 wrtLock
    void writeUnlock();   // 写者释放锁：直接释放 wrtLock
};
#endif