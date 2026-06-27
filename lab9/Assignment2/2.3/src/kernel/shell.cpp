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
    const char *names[5] = {
        "alpha.txt",
        "beta.txt",
        "gamma.txt",
        "delta.txt",
        "echo.txt"};
    const char *contents[5] = {
        "alpha: first file content",
        "beta: second file content",
        "gamma: third file content",
        "delta: fourth file content",
        "echo: fifth file content"};

    printf("\n==============================================\n");
    printf("      Assignment2 FAT16 File Operation Test\n");
    printf("==============================================\n\n");

    // ---- 步骤1: 格式化文件系统 ----
    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    // ---- 步骤2: 查看文件系统信息 ----
    printf("[Step 2] File system info:\n");
    fs->showInfo();
    printf("\n");

    // 使用数组统一驱动 5 个文件的创建、写入和读取，避免手写重复流程。
    // ---- 步骤3: 创建 5 个文件 ----
    printf("[Step 3] Creating five files...\n");
    for (int i = 0; i < 5; ++i)
    {
        fs->createFile(names[i]);
        printf("  Created: %s\n", names[i]);
    }
    printf("\n");

    // ---- 步骤4: 写入不同内容 ----
    printf("[Step 4] Writing different content to each file...\n");
    for (int i = 0; i < 5; ++i)
    {
        fs->writeFile(names[i], contents[i], strlen(contents[i]));
        printf("  Wrote %d bytes to %s\n", strlen(contents[i]), names[i]);
    }
    printf("\n");

    // ---- 步骤5: 列出目录，检查文件名和大小 ----
    printf("[Step 5] Listing files after initial writes:\n");
    fs->listFiles();
    printf("\n");

    // ---- 步骤6: 逐个读取文件，验证内容一致 ----
    printf("[Step 6] Reading all files back:\n");
    for (int i = 0; i < 5; ++i)
    {
        // 每次读取前清空缓冲区，确保打印出的内容只来自当前文件。
        memset(readBuf, 0, sizeof(readBuf));
        int n = fs->readFile(names[i], readBuf, sizeof(readBuf) - 1);
        if (n > 0)
        {
            readBuf[n] = '\0';
            printf("  %s (%d bytes): %s\n", names[i], n, readBuf);
        }
    }
    printf("\n");

    // ---- 步骤7: 删除部分文件 ----
    printf("[Step 7] Deleting beta.txt and delta.txt...\n");
    fs->deleteFile("beta.txt");
    printf("  Deleted: beta.txt\n");
    fs->deleteFile("delta.txt");
    printf("  Deleted: delta.txt\n");
    printf("\n");

    // ---- 步骤8: 删除后列出目录 ----
    printf("[Step 8] Listing files after delete:\n");
    fs->listFiles();
    printf("\n");

    // ---- 步骤9: 重建同名文件并写入新内容 ----
    printf("[Step 9] Recreating deleted names with new content...\n");
    // 重新创建 beta/delta 用于验证 0xE5 删除目录项和已释放簇都能被复用。
    const char *beta2 = "beta: recreated content";
    const char *delta2 = "delta: recreated content";
    fs->createFile("beta.txt");
    fs->writeFile("beta.txt", beta2, strlen(beta2));
    printf("  Recreated beta.txt with %d bytes\n", strlen(beta2));
    fs->createFile("delta.txt");
    fs->writeFile("delta.txt", delta2, strlen(delta2));
    printf("  Recreated delta.txt with %d bytes\n", strlen(delta2));
    printf("\n");

    // ---- 步骤10: 再次列出目录，观察目录项和簇复用 ----
    printf("[Step 10] Listing files after recreate:\n");
    fs->listFiles();
    printf("\n");

    // ---- 步骤11: 读取重建文件，验证新内容 ----
    printf("[Step 11] Reading recreated files:\n");
    memset(readBuf, 0, sizeof(readBuf));
    int n = fs->readFile("beta.txt", readBuf, sizeof(readBuf) - 1);
    if (n > 0)
    {
        readBuf[n] = '\0';
        printf("  beta.txt (%d bytes): %s\n", n, readBuf);
    }
    memset(readBuf, 0, sizeof(readBuf));
    n = fs->readFile("delta.txt", readBuf, sizeof(readBuf) - 1);
    if (n > 0)
    {
        readBuf[n] = '\0';
        printf("  delta.txt (%d bytes): %s\n", n, readBuf);
    }
    printf("\n");

    // ---- 步骤12: 最终文件系统信息 ----
    printf("[Step 12] Final file system info:\n");
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
