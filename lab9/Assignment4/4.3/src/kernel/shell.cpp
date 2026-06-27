#include "shell.h"
#include "asm_utils.h"
#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

Shell::Shell()
{
    disk = nullptr;
    fs = nullptr;
}

void Shell::initialize(DiskDriver *drv, FAT16 *filesystem)
{
    this->disk = drv;
    this->fs = filesystem;
}

void Shell::printLogo()
{
    move_cursor(0, 19);
    printf(" ____  _   _ __  __ __  __ _____ ____\n");
    move_cursor(1, 19);
    printf("/ ___|| | | |  \\/  |  \\/  | ____|  _ \\\n");
    move_cursor(2, 19);
    printf("\\___ \\| | | | |\\/| | |\\/| |  _| | |_) |\n");
    move_cursor(3, 19);
    printf(" ___) | |_| | |  | | |  | | |___|  _ <\n");
    move_cursor(4, 19);
    printf("|____/ \\___/|_|  |_|_|  |_|_____|_| \\_\\\n");
}

void Shell::runDemo()
{
    static char bigData[1501];
    static char bigRead[1600];
    for (int i = 0; i < 1500; ++i)
        bigData[i] = 'A' + (i % 26);
    bigData[1500] = '\0';

    printf("\n==============================================\n");
    printf("      Assignment4.3 Large File and Batch I/O Test\n");
    printf("==============================================\n\n");

    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    printf("[Step 2] Normal write/read of a 1500-byte file...\n");
    disk->resetCounters();
    fs->writeFile("normal.bin", bigData, 1500);
    printf("  normal write commands: %d\n", disk->getCommandCount());
    printf("  normal write word IO : %d\n", disk->getWordIOCount());

    memset(bigRead, 0, sizeof(bigRead));
    disk->resetCounters();
    int n = fs->readFile("normal.bin", bigRead, sizeof(bigRead) - 1);
    bigRead[n] = '\0';
    printf("  normal read bytes    : %d\n", n);
    printf("  normal read commands : %d\n", disk->getCommandCount());
    printf("  normal read word IO  : %d\n", disk->getWordIOCount());
    printf("\n");

    printf("[Step 3] Optimized write/read of another 1500-byte file...\n");
    disk->resetCounters();
    fs->writeFileOptimized("opt.bin", bigData, 1500);
    printf("  optimized write commands: %d\n", disk->getCommandCount());
    printf("  optimized write word IO : %d\n", disk->getWordIOCount());

    memset(bigRead, 0, sizeof(bigRead));
    disk->resetCounters();
    n = fs->readFileOptimized("opt.bin", bigRead, sizeof(bigRead) - 1);
    bigRead[n] = '\0';
    printf("  optimized read bytes    : %d\n", n);
    printf("  optimized read commands : %d\n", disk->getCommandCount());
    printf("  optimized read word IO  : %d\n", disk->getWordIOCount());
    printf("  content check first/last ASCII: %d %d\n", bigRead[0], bigRead[1499]);
    printf("\n");

    printf("[Step 4] Listing files:\n");
    fs->listFiles();
    printf("\n");

    printf("[Step 5] Final file system info:\n");
    fs->showInfo();
    printf("\n");

    printf("==============================================\n");
    printf("      Demo Complete!\n");
    printf("==============================================\n");
}

void Shell::run()
{
    // 清屏
    move_cursor(0, 0);
    for (int i = 0; i < 25; ++i)
    {
        for (int j = 0; j < 80; ++j)
        {
            printf(" ");
        }
    }
    move_cursor(0, 0);

    printLogo();

    move_cursor(6, 20);
    printf("SUMMER OS - FAT16 File System Lab\n\n");

    // 运行文件系统演示
    runDemo();

    asm_halt();
}
