#ifndef SYNC_H
#define SYNC_H

#include "os_type.h"
#include "list.h"
#include "thread.h"

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

// ============ 管程：互斥锁 ============
class MonitorMutex
{
private:
    SpinLock lock;
    bool locked;
    PCB *owner;

public:
    MonitorMutex();
    void initialize();
    void acquire(); // 获取管程互斥锁
    void release(); // 释放管程互斥锁
};

// ============ 管程：条件变量 ============
class MonitorCondition
{
private:
    List waiting;   // 等待在该条件上的线程队列

public:
    MonitorCondition();
    void initialize();

    // wait: 释放管程锁，阻塞自己；被唤醒后重新获取锁
    void wait(MonitorMutex *monLock);

    // signal: 唤醒一个等待线程（MESA 语义）
    void signal();
};
#endif