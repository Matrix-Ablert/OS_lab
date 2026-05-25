#include "sync.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"
#include "interrupt.h"

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

// ==========================================
// ============ MonitorMutex 实现 ============
// ==========================================

MonitorMutex::MonitorMutex()
{
    initialize();
}

void MonitorMutex::initialize()
{
    lock.initialize();
    locked = false;
    owner = nullptr;
}

void MonitorMutex::acquire()
{
    // 使用自旋锁实现互斥获取
    lock.lock();
    locked = true;
    owner = programManager.running;
    // 自旋锁保持 locked 状态，直到 release
}

void MonitorMutex::release()
{
    locked = false;
    owner = nullptr;
    lock.unlock();
}

// ===============================================
// ============ MonitorCondition 实现 ============
// ===============================================

MonitorCondition::MonitorCondition()
{
    initialize();
}

void MonitorCondition::initialize()
{
    waiting.initialize();
}

void MonitorCondition::wait(MonitorMutex *monLock)
{
    PCB *cur = nullptr;
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 将自己加入条件等待队列
    cur = programManager.running;
    waiting.push_back(&(cur->tagInGeneralList));
    cur->status = ProgramStatus::BLOCKED;

    // 释放管程锁（让其他线程可以进入管程）
    monLock->release();

    interruptManager.setInterruptStatus(status);

    // 阻塞自己
    programManager.schedule();

    // ===== 被唤醒后重新获取管程锁 =====
    monLock->acquire();
}

void MonitorCondition::signal()
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    if (waiting.size() > 0)
    {
        PCB *program = ListItem2PCB(waiting.front(), tagInGeneralList);
        waiting.pop_front();
        programManager.MESA_WakeUp(program);
    }

    interruptManager.setInterruptStatus(status);
}