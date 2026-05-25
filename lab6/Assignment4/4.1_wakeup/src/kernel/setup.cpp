#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"

STDIO stdio;
InterruptManager interruptManager;
ProgramManager programManager;

#define BUFFER_SIZE 3
#define PRODUCER_COUNT 3
#define CONSUMER_COUNT 3
#define ITEMS_PER_THREAD 5

int buffer[BUFFER_SIZE];
int in = 0, out = 0, count = 0;
Semaphore mutex;
Condition notFull;
Condition notEmpty;

// ============ 缓冲区内操作（已持有 mutex）============
void put_item(int item) {
    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;
    count++;
}

int get_item() {
    int item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    count--;
    return item;
}

// ============ 生产者线程 ============
void producer(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        mutex.P();

        while (count == BUFFER_SIZE) {
            printf("[P%d] buffer full (cnt=%d), waiting...\n", id, count);
            notFull.wait(&mutex);
        }

        int item = id * 100 + i;
        put_item(item);
        printf("[P%d] produce item=%d at[%d] cnt=%d\n",
               id, item, in - 1 >= 0 ? in - 1 : BUFFER_SIZE - 1, count);

        if (count == 1) {
            notEmpty.signal();
        }
        mutex.V();

        int delay = 0xfffff;
        while (delay) --delay;
    }
    printf(">> [P%d] DONE\n", id);
}

// ============ 消费者线程 ============
void consumer(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        mutex.P();

        while (count == 0) {
            printf("[C%d] buffer empty (cnt=%d), waiting...\n", id, count);
            notEmpty.wait(&mutex);
        }

        int item = get_item();
        printf("[C%d] consume item=%d from[%d] cnt=%d\n",
               id, item, out - 1 >= 0 ? out - 1 : BUFFER_SIZE - 1, count + 1);

        if (count == BUFFER_SIZE - 1) {
            notFull.signal();
        }
        mutex.V();

        int delay = 0xfffff;
        while (delay) --delay;
    }
    printf(">> [C%d] DONE\n", id);
}

// ============ 主线程：创建并启动测试 ============
void first_thread(void *arg) {
    stdio.moveCursor(0);
    for (int i = 0; i < 25 * 80; ++i) stdio.print(' ');
    stdio.moveCursor(0);

    printf("========================================\n");
    printf("    Wake-Up Model Comparison Test\n");
    printf("========================================\n\n");

    // ===== 依次测试三种模型 =====

    // ---------- MESA ----------
    printf("=== [MESA] Model ===\n");
    printf("Expect: signaler keeps running;\n"
           "        woken thread enters ready queue.\n\n");

    in = 0; out = 0; count = 0;
    mutex.initialize(1);
    notFull.initialize(MESA_MODEL);
    notEmpty.initialize(MESA_MODEL);

    for (int i = 0; i < PRODUCER_COUNT; i++)
        programManager.executeThread(producer, (void *)i, "prod", 1);
    for (int i = 0; i < CONSUMER_COUNT; i++)
        programManager.executeThread(consumer, (void *)i, "cons", 1);

    asm_halt();
}

extern "C" void setup_kernel()
{
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    stdio.initialize();
    programManager.initialize();

    int pid = programManager.executeThread(first_thread, nullptr, "first", 1);
    if (pid == -1) {
        printf("can not execute thread\n");
        asm_halt();
    }

    ListItem *item = programManager.readyPrograms.front();
    PCB *firstThread = ListItem2PCB(item, tagInGeneralList);
    firstThread->status = RUNNING;
    programManager.readyPrograms.pop_front();
    programManager.running = firstThread;
    asm_switch_thread(0, firstThread);

    asm_halt();
}
