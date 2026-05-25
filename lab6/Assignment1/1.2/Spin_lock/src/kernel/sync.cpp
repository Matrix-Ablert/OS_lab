#include "sync.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"

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

// ============ 基于 lock cmpxchg 的 CAS 自旋锁实现 ============

SpinLockCAS::SpinLockCAS()
{
    initialize();
}

void SpinLockCAS::initialize()
{
    bolt = 0;
}

void SpinLockCAS::lock()
{
    // CAS 自旋：尝试将 bolt 从 0(期望值) 改为 1(新值)
    // 若 bolt 当前 ≠ 0，说明锁已被占用，继续自旋
    while (asm_lock_cmpxchg(&bolt, 0, 1) != 0)
    {
        // 空转自旋，等待锁释放
    }
}

void SpinLockCAS::unlock()
{
    bolt = 0;
}