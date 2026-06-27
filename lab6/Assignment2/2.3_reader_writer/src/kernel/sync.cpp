#include "sync.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"

SpinLock::SpinLock()
{
    initialize();
}

void SpinLock::initialize()
{
    bolt = 0;
}

void SpinLock::lock()
{
    uint32 key = 1;

    do
    {
        asm_atomic_exchange(&key, &bolt);
        //printf("pid: %d\n", programManager.running->pid);
    } while (key);
}

void SpinLock::unlock()
{
    bolt = 0;
}

Semaphore::Semaphore()
{
    initialize(0);
}

void Semaphore::initialize(uint32 counter)
{
    this->counter = counter;
    semLock.initialize();
    waiting.initialize();
}

void Semaphore::P()
{
    PCB *cur = nullptr;

    while (true)
    {
        semLock.lock();
        if (counter > 0)
        {
            --counter;
            semLock.unlock();
            return;
        }

        cur = programManager.running;
        waiting.push_back(&(cur->tagInGeneralList));
        cur->status = ProgramStatus::BLOCKED;

        semLock.unlock();
        programManager.schedule();
    }
}

void Semaphore::V()
{
    semLock.lock();
    ++counter;
    if (waiting.size())
    {
        PCB *program = ListItem2PCB(waiting.front(), tagInGeneralList);
        waiting.pop_front();
        semLock.unlock();
        programManager.MESA_WakeUp(program);
    }
    else
    {
        semLock.unlock();
    }
}

RWLock::RWLock()
{
    initialize();
}

void RWLock::initialize()
{
    mutex.initialize(1);     // 互斥量初始为 1，保证 readCount 原子访问
    wrtLock.initialize(1);   // 写者锁初始为 1，表示写者可进入
    readCount = 0;           // 初始无读者
}

/**
 * 读者获取读锁（readLock）
 * ---------------------------------------------------------------
 * 算法流程：
 *   1. P(mutex)       — 互斥地操作 readCount
 *   2. readCount++    — 读者计数 +1
 *   3. 若 readCount == 1（第一个读者）
 *      → P(wrtLock)   — 占有写者锁，阻塞写者
 *      这是"读者优先"的关键：一旦有读者在临界区，写者就被挡在门外
 *   4. V(mutex)       — 释放 mutex，后续读者可以进来
 *
 * 为什么后续读者可以"插队"？
 *   后续读者只需要 P(mutex)、readCount++、V(mutex)，
 *   完全不需要碰 wrtLock（因为 readCount 已经 > 1），
 *   所以读者可以源源不断地加入，而写者始终被 wrtLock 挡在外面。
 * ---------------------------------------------------------------
 */
void RWLock::readLock()
{
    mutex.P();                   // ① 互斥进入，保护 readCount
    ++readCount;                 // ② 读者计数递增
    if (readCount == 1)          // ③ 第一个读者：拿走写者锁
    {
        wrtLock.P();             //    此时写者无法进入临界区
    }
    mutex.V();                   // ④ 释放互斥量，其他读者可以进来
}

/**
 * 读者释放读锁（readUnlock）
 * ---------------------------------------------------------------
 * 算法流程：
 *   1. P(mutex)       — 互斥地操作 readCount
 *   2. readCount--    — 读者计数 -1
 *   3. 若 readCount == 0（最后一个读者离开）
 *      → V(wrtLock)   — 释放写者锁，唤醒等待中的写者
 *   4. V(mutex)       — 释放 mutex
 *
 * 这就是饥饿的"解除点"：只有所有读者都离开后，写者才有机会。
 * ---------------------------------------------------------------
 */
void RWLock::readUnlock()
{
    mutex.P();                   // ① 互斥进入
    --readCount;                 // ② 读者计数递减
    if (readCount == 0)          // ③ 最后一个读者：归还写者锁
    {
        wrtLock.V();             //    唤醒阻塞的写者
    }
    mutex.V();                   // ④ 释放互斥量
}

/**
 * 写者获取写锁（writeLock）
 * ---------------------------------------------------------------
 * 写者的逻辑非常简单：直接竞争 wrtLock。
 * - 如果 wrtLock 可用（无读者也无其他写者）→ 进入
 * - 如果 wrtLock 被占有 → 阻塞直到被释放
 * ---------------------------------------------------------------
 */
void RWLock::writeLock()
{
    wrtLock.P();                 // 阻塞直到获取写者锁
}

/**
 * 写者释放写锁（writeUnlock）
 * ---------------------------------------------------------------
 * 释放 wrtLock 后，可能唤醒：
 * - 一个等待的写者（如果写者队列非空）
 * - 或者一群读者中的第一个（触发 readCount 0→1，重新占住 wrtLock）
 * ---------------------------------------------------------------
 */
void RWLock::writeUnlock()
{
    wrtLock.V();                 // 释放写者锁
}