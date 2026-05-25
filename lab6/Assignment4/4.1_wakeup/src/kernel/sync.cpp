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
// ============ Condition 实现 ============
// ==========================================

Condition::Condition()
{
    initialize(MESA_MODEL);
}

void Condition::initialize(WakeUpModel model)
{
    condLock.initialize();
    waiting.initialize();
    wakeModel = model;
}

void Condition::wait(Semaphore *mutex)
{
    PCB *cur = nullptr;

    condLock.lock();

    // 将当前线程加入条件等待队列
    cur = programManager.running;
    waiting.push_back(&(cur->tagInGeneralList));
    cur->status = ProgramStatus::BLOCKED;

    condLock.unlock();

    // 释放 mutex，允许其他线程进入临界区
    mutex->V();

    // 阻塞自己，调度其他线程
    programManager.schedule();

    // ===== 被唤醒后，重新获取 mutex =====
    mutex->P();
}

void Condition::signal()
{
    PCB *woken = nullptr;

    condLock.lock();

    if (waiting.size() > 0)
    {
        woken = ListItem2PCB(waiting.front(), tagInGeneralList);
        waiting.pop_front();
        condLock.unlock();

        // 根据唤醒模型选择不同的唤醒方法
        switch (wakeModel)
        {
            case MESA_MODEL:
                programManager.MESA_WakeUp(woken);
                break;
            case HOARE_MODEL:
                // Hoare: signal() 后立即切换到被唤醒者
                // 被唤醒者直接在临界区内继续，无需重新获取 mutex
                programManager.Hoare_WakeUp(woken);
                break;
            case HASEN_MODEL:
                // Hasen: 信号者继续执行完临界区
                // 被唤醒者保证是下一个运行的
                programManager.Hasen_WakeUp(woken);
                break;
        }
    }
    else
    {
        condLock.unlock();
    }
}