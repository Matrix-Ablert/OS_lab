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
    printf("\n==============================================\n");
    printf("      Assignment4.4 FSCK Consistency Test\n");
    printf("==============================================\n\n");

    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    printf("[Step 2] Creating a clean file system state...\n");
    fs->writeFile("ok.txt", "consistent", strlen("consistent"));
    fs->listFiles();
    printf("\n");

    printf("[Step 3] Running fsck on clean state:\n");
    fs->fsck();
    printf("\n");

    printf("[Step 4] Injecting test inconsistencies...\n");
    fs->injectFsckTestInconsistency();
    printf("\n");

    printf("[Step 5] Listing files after injection:\n");
    fs->listFiles();
    printf("\n");

    printf("[Step 6] Running fsck after injected errors:\n");
    fs->fsck();
    printf("\n");

    printf("[Step 7] Final file system info:\n");
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
