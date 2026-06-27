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
    printf("      Assignment3.3 renameFile Test\n");
    printf("==============================================\n\n");

    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    printf("[Step 2] Creating source and conflict files...\n");
    const char *msg = "rename keeps file data and FAT chain";
    fs->writeFile("old.txt", msg, strlen(msg));
    fs->writeFile("other.txt", "conflict target", strlen("conflict target"));
    printf("  Created old.txt and other.txt\n");
    printf("\n");

    printf("[Step 3] Listing files before rename:\n");
    fs->listFiles();
    printf("\n");

    printf("[Step 4] Renaming old.txt to renamed.txt...\n");
    if (fs->renameFile("old.txt", "renamed.txt"))
        printf("  Rename success: old.txt -> renamed.txt\n");
    printf("\n");

    printf("[Step 5] Listing files after rename:\n");
    fs->listFiles();
    printf("\n");

    printf("[Step 6] Reading renamed file:\n");
    memset(readBuf, 0, sizeof(readBuf));
    int n = fs->readFile("renamed.txt", readBuf, sizeof(readBuf) - 1);
    if (n > 0)
    {
        readBuf[n] = '\0';
        printf("  renamed.txt (%d bytes): %s\n", n, readBuf);
    }

    printf("[Step 7] Reading old name should fail:\n");
    // 旧名字已不在根目录中，读取失败说明目录项名称确实被替换。
    fs->readFile("old.txt", readBuf, sizeof(readBuf) - 1);
    printf("\n");

    printf("[Step 8] Renaming to an existing name should fail:\n");
    // other.txt 已存在，用它作为新名字应被 renameFile 的冲突检查拒绝。
    if (!fs->renameFile("renamed.txt", "other.txt"))
        printf("  Conflict rename correctly failed\n");
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
