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
    char readBuf[1024];

    printf("\n==============================================\n");
    printf("      Assignment4.2 Subdirectory Test\n");
    printf("==============================================\n\n");

    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    printf("[Step 2] Creating directory docs...\n");
    fs->mkdir("docs");
    printf("  mkdir docs\n");
    printf("\n");

    printf("[Step 3] Creating and writing docs/readme.txt...\n");
    const char *msg = "hello from a subdirectory";
    fs->writeFile("docs/readme.txt", msg, strlen(msg));
    printf("  wrote docs/readme.txt (%d bytes)\n", strlen(msg));
    printf("\n");

    printf("[Step 4] Listing root directory:\n");
    fs->listFiles();
    printf("\n");

    printf("[Step 5] Listing docs directory:\n");
    fs->listFiles("docs");
    printf("\n");

    printf("[Step 6] Reading docs/readme.txt:\n");
    memset(readBuf, 0, sizeof(readBuf));
    int n = fs->readFile("docs/readme.txt", readBuf, sizeof(readBuf) - 1);
    if (n > 0)
    {
        readBuf[n] = '\0';
        printf("  docs/readme.txt (%d bytes): %s\n", n, readBuf);
    }
    printf("\n");

    printf("[Step 7] rmdir docs should fail while not empty:\n");
    fs->rmdir("docs");
    printf("\n");

    printf("[Step 8] Delete child file and remove docs:\n");
    fs->deleteFile("docs/readme.txt");
    printf("  deleted docs/readme.txt\n");
    fs->rmdir("docs");
    printf("  rmdir docs\n");
    printf("\n");

    printf("[Step 9] Final file system info:\n");
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
