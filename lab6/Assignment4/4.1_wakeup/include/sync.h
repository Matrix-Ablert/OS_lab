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

// ============ 唤醒模型枚举 ============
enum WakeUpModel
{
    MESA_MODEL,   // MESA 语义：唤醒者继续，被唤醒者进就绪队列
    HOARE_MODEL,  // Hoare 语义：立即切换给被唤醒者
    HASEN_MODEL   // Hasen 语义：保证被唤醒者是下一个运行
};

// ============ 条件变量 (Condition) ============
class Condition
{
private:
    List waiting;            // 等待在该条件上的线程队列
    SpinLock condLock;       // 保护等待队列的锁
    WakeUpModel wakeModel;   // 使用的唤醒模型

public:
    Condition();
    void initialize(WakeUpModel model = MESA_MODEL);

    // wait: 原子地释放 mutex 并阻塞，被唤醒后重新获取 mutex
    void wait(Semaphore *mutex);

    // signal: 唤醒一个等待线程（根据模型不同，行为不同）
    void signal();
};
#endif