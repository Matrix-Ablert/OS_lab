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

class RWLock
{
private:
    Semaphore mutex;
    Semaphore wrtLock;
    int readCount;

public:
    RWLock();
    void initialize();
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();
};
#endif