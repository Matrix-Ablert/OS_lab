#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"
#include "syscall.h"
#include "tss.h"
#include "shell.h"
#include "disk.h"
#include "fat16.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;
// 内存管理器
MemoryManager memoryManager;
// 内核字节级内存管理器
ByteMemoryManager kernelByteMemoryManager;
// 系统调用
SystemService systemService;
// Task State Segment
TSS tss;
// 磁盘驱动
DiskDriver diskDriver;
// FAT16 文件系统
FAT16 fat16;

int syscall_0(int first, int second, int third, int forth, int fifth)
{
    printf("system call 0: %d, %d, %d, %d, %d\n",
           first, second, third, forth, fifth);
    return first + second + third + forth + fifth;
}

int testsTotal = 0;
int testsPassed = 0;

void report_test(const char *name, bool ok)
{
    ++testsTotal;
    if (ok)
    {
        ++testsPassed;
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
    }
}

void fill_pattern(char *buffer, int size, int seed)
{
    for (int i = 0; i < size; ++i)
    {
        buffer[i] = (char)((i + seed) & 0x7f);
    }
}

bool verify_pattern(char *buffer, int size, int seed)
{
    for (int i = 0; i < size; ++i)
    {
        if (buffer[i] != (char)((i + seed) & 0x7f))
        {
            return false;
        }
    }
    return true;
}

bool test_one_allocation(int size, int seed)
{
    char *buffer = (char *)malloc(size);
    if (!buffer)
    {
        return false;
    }

    fill_pattern(buffer, size, seed);
    bool ok = verify_pattern(buffer, size, seed);
    free(buffer);
    return ok;
}

void run_malloc_tests()
{
    printf("\n==============================================\n");
    printf("      Project2 malloc/free/realloc Test\n");
    printf("==============================================\n\n");

    report_test("malloc(0) returns null", malloc(0) == nullptr);

    report_test("small block 16 bytes", test_one_allocation(16, 1));
    report_test("small block 17 bytes", test_one_allocation(17, 2));
    report_test("small block 64 bytes", test_one_allocation(64, 3));
    report_test("small block 1000 bytes", test_one_allocation(1000, 4));
    report_test("small block 1024 bytes", test_one_allocation(1024, 5));

    report_test("large block 1025 bytes", test_one_allocation(1025, 6));
    report_test("large block 4096 bytes", test_one_allocation(4096, 7));
    report_test("large block 6000 bytes", test_one_allocation(6000, 8));

    char *a = (char *)malloc(64);
    char *b = (char *)malloc(64);
    char *c = (char *)malloc(64);
    bool reuseOk = a && b && c;
    if (reuseOk)
    {
        fill_pattern(a, 64, 11);
        fill_pattern(b, 64, 12);
        fill_pattern(c, 64, 13);
        free(b);
        char *d = (char *)malloc(64);
        reuseOk = (d == b);
        if (d)
        {
            fill_pattern(d, 64, 14);
            reuseOk = reuseOk && verify_pattern(a, 64, 11) && verify_pattern(c, 64, 13);
            free(d);
        }
        free(a);
        free(c);
    }
    report_test("free reuses returned block without data corruption", reuseOk);

    void *blocks[127];
    bool arenaOk = true;
    for (int i = 0; i < 127; ++i)
    {
        blocks[i] = nullptr;
    }
    for (int i = 0; i < 127; ++i)
    {
        blocks[i] = malloc(32);
        if (!blocks[i])
        {
            arenaOk = false;
            break;
        }
    }
    for (int i = 0; i < 127; ++i)
    {
        if (blocks[i])
        {
            free(blocks[i]);
        }
    }
    char *afterArenaRelease = (char *)malloc(32);
    arenaOk = arenaOk && afterArenaRelease;
    if (afterArenaRelease)
    {
        fill_pattern(afterArenaRelease, 32, 15);
        arenaOk = arenaOk && verify_pattern(afterArenaRelease, 32, 15);
        free(afterArenaRelease);
    }
    report_test("arena page is reusable after all blocks are freed", arenaOk);

    char *grow = (char *)malloc(32);
    bool growOk = grow;
    if (grow)
    {
        fill_pattern(grow, 32, 21);
        char *grown = (char *)realloc(grow, 2000);
        growOk = grown && verify_pattern(grown, 32, 21);
        if (grown)
        {
            fill_pattern(grown + 32, 1968, 22);
            growOk = growOk && verify_pattern(grown + 32, 1968, 22);
            free(grown);
        }
    }
    report_test("realloc grows and preserves old data", growOk);

    char *shrink = (char *)malloc(512);
    bool shrinkOk = shrink;
    if (shrink)
    {
        fill_pattern(shrink, 512, 31);
        char *shrunk = (char *)realloc(shrink, 64);
        shrinkOk = (shrunk == shrink) && verify_pattern(shrunk, 64, 31);
        free(shrunk);
    }
    report_test("realloc shrinks in place and preserves prefix", shrinkOk);

    char *fromNull = (char *)realloc(nullptr, 128);
    bool nullOk = fromNull;
    if (fromNull)
    {
        fill_pattern(fromNull, 128, 41);
        nullOk = verify_pattern(fromNull, 128, 41);
        fromNull = (char *)realloc(fromNull, 0);
        nullOk = nullOk && (fromNull == nullptr);
    }
    report_test("realloc null and zero-size cases", nullOk);

    printf("\nSummary: %d/%d tests passed\n", testsPassed, testsTotal);
    printf("==============================================\n");
}

void first_process()
{
    int pid = fork();

    if (pid == -1)
    {
        printf("error\n");
        asm_halt();
    }
    else
    {
        if (pid)
        {
            while ((pid = wait(nullptr)) != -1)
            {
            }
            asm_halt();
        }
        else
        {
            run_malloc_tests();
            exit(testsTotal - testsPassed);
        }
    }
}

void first_thread(void *arg)
{
    // 初始化磁盘驱动
    diskDriver.initialize();

    printf("start process\n");
    programManager.executeProcess((const char *)first_process, 1);
    asm_halt();
}

extern "C" void setup_kernel()
{
    // 中断管理器
    interruptManager.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    // 输出管理器
    stdio.initialize();

    // 进程/线程管理器
    programManager.initialize();

    // 初始化系统调用
    systemService.initialize();
    systemService.setSystemCall(0, (int)syscall_0);
    systemService.setSystemCall(1, (int)syscall_write);
    systemService.setSystemCall(2, (int)syscall_fork);
    systemService.setSystemCall(3, (int)syscall_exit);
    systemService.setSystemCall(4, (int)syscall_wait);
    systemService.setSystemCall(5, (int)syscall_move_cursor);
    systemService.setSystemCall(6, (int)syscall_malloc);
    systemService.setSystemCall(7, (int)syscall_free);
    systemService.setSystemCall(8, (int)syscall_realloc);

    // 内存管理器
    memoryManager.initialize();
    kernelByteMemoryManager.initialize();

    // 创建第一个线程
    int pid = programManager.executeThread(first_thread, nullptr, "first thread", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }

    ListItem *item = programManager.readyPrograms.front();
    PCB *firstThread = ListItem2PCB(item, tagInGeneralList);
    firstThread->status = ProgramStatus::RUNNING;
    programManager.readyPrograms.pop_front();
    programManager.running = firstThread;
    asm_switch_thread(0, firstThread);

    asm_halt();
}
