#include "program.h"
#include "stdlib.h"
#include "interrupt.h"
#include "asm_utils.h"
#include "stdio.h"
#include "thread.h"
#include "os_modules.h"

const int PCB_SIZE = 4096;                   // PCB的大小，4KB。
char PCB_SET[PCB_SIZE * MAX_PROGRAM_AMOUNT]; // 存放PCB的数组，预留了MAX_PROGRAM_AMOUNT个PCB的大小空间。
bool PCB_SET_STATUS[MAX_PROGRAM_AMOUNT];     // PCB的分配状态，true表示已经分配，false表示未分配。

ProgramManager::ProgramManager()
{
    initialize();
}

void ProgramManager::initialize()
{
    allPrograms.initialize();
    readyPrograms.initialize();
    running = nullptr;

    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        PCB_SET_STATUS[i] = false;
    }
}

int ProgramManager::executeThread(ThreadFunction function, void *parameter, const char *name, int priority)
{
    // 关中断，防止创建线程的过程被打断
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    // 分配一页作为PCB
    PCB *thread = allocatePCB();

    if (!thread)
        return -1;

    // 初始化分配的页
    memset(thread, 0, PCB_SIZE);

    for (int i = 0; i < MAX_PROGRAM_NAME && name[i]; ++i)
    {
        thread->name[i] = name[i];
    }

    thread->status = ProgramStatus::READY;
    thread->priority = priority;
    thread->ticks = priority * 10;
    thread->ticksPassedBy = 0;
    thread->pid = ((int)thread - (int)PCB_SET) / PCB_SIZE;

    // 线程栈
    thread->stack = (int *)((int)thread + PCB_SIZE);
    thread->stack -= 7;
    thread->stack[0] = 0;
    thread->stack[1] = 0;
    thread->stack[2] = 0;
    thread->stack[3] = 0;
    thread->stack[4] = (int)function;
    thread->stack[5] = (int)program_exit;
    thread->stack[6] = (int)parameter;

    allPrograms.push_back(&(thread->tagInAllList));
    readyPrograms.push_back(&(thread->tagInGeneralList));

    // 恢复中断
    interruptManager.setInterruptStatus(status);

    return thread->pid;
}

void ProgramManager::schedule()
{
    bool status = interruptManager.getInterruptStatus();
    interruptManager.disableInterrupt();

    if (readyPrograms.size() == 0)
    {
        interruptManager.setInterruptStatus(status);
        return;
    } else {
        //printf("amount of ready programs: %d\n", readyPrograms.size());
    }

    if (running->status == ProgramStatus::RUNNING)
    {
        running->status = ProgramStatus::READY;
        running->ticks = running->priority * 10;
        readyPrograms.push_back(&(running->tagInGeneralList));
    }
    else if (running->status == ProgramStatus::DEAD)
    {
        releasePCB(running);
    }

    ListItem *item = readyPrograms.front();
    PCB *next = ListItem2PCB(item, tagInGeneralList);
    PCB *cur = running;
    next->status = ProgramStatus::RUNNING;
    running = next;
    readyPrograms.pop_front();

    asm_switch_thread(cur, next);

    interruptManager.setInterruptStatus(status);
}

void program_exit()
{
    PCB *thread = programManager.running;
    thread->status = ProgramStatus::DEAD;

    if (thread->pid)
    {
        programManager.schedule();
    }
    else
    {
        interruptManager.disableInterrupt();
        printf("halt\n");
        asm_halt();
    }
}

PCB *ProgramManager::allocatePCB()
{
    for (int i = 0; i < MAX_PROGRAM_AMOUNT; ++i)
    {
        if (!PCB_SET_STATUS[i])
        {
            PCB_SET_STATUS[i] = true;
            return (PCB *)((int)PCB_SET + PCB_SIZE * i);
        }
    }

    return nullptr;
}

void ProgramManager::releasePCB(PCB *program)
{
    int index = ((int)program - (int)PCB_SET) / PCB_SIZE;
    PCB_SET_STATUS[index] = false;
}

// ============ MESA 模型 ============
// 唤醒者继续执行，被唤醒者放入就绪队列队首
void ProgramManager::MESA_WakeUp(PCB *program) {
    program->status = ProgramStatus::READY;
    readyPrograms.push_front(&(program->tagInGeneralList));
}

// ============ Hoare 模型 ============
// 唤醒者立即切换给被唤醒者：
//   1. 唤醒者将自己放入就绪队列队首（状态 READY）
//   2. 立即通过 asm_switch_thread 切换到被唤醒者
//   3. 被唤醒者返回后，唤醒者从就绪队首被调度回来继续执行
void ProgramManager::Hoare_WakeUp(PCB *program) {
    PCB *signal_sender = running;

    // 将唤醒者放入就绪队列队首（等待被唤醒者完成后恢复）
    signal_sender->status = ProgramStatus::READY;
    readyPrograms.push_front(&(signal_sender->tagInGeneralList));

    // 立即切换：被唤醒者变为 RUNNING
    program->status = ProgramStatus::RUNNING;
    running = program;

    // 上下文切换：从唤醒者 → 被唤醒者
    // 当被唤醒者调用 schedule() 放弃 CPU 后，
    // signal_sender 在队首会被优先调度
    asm_switch_thread(signal_sender, program);
}

// ============ Hasen 模型 ============
// 唤醒者继续执行，但被唤醒者强制排在就绪队列最前面
// 保证当前线程放弃 CPU 后，一定是被唤醒者运行
void ProgramManager::Hasen_WakeUp(PCB *program) {
    program->status = ProgramStatus::READY;
    // push_front 保证被唤醒者在就绪队列最前面
    readyPrograms.push_front(&(program->tagInGeneralList));
}