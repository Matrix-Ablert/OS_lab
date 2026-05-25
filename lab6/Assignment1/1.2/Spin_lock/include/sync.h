#ifndef SYNC_H
#define SYNC_H

#include "os_type.h"

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

// 基于 lock cmpxchg 的 CAS 自旋锁
class SpinLockCAS
{
private:
    uint32 bolt;
public:
    SpinLockCAS();
    void initialize();
    void lock();
    void unlock();
};
#endif