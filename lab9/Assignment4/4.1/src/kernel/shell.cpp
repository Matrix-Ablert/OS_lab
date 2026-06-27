#include "shell.h"
#include "asm_utils.h"
#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

extern bool keyboard_read_line(char *buffer);

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
    printf("      Assignment4.1 Interactive Shell Test\n");
    printf("==============================================\n\n");

    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    const char *commands[9] = {
        "help",
        "touch note.txt",
        "write note.txt hello-shell",
        "ls",
        "cat note.txt",
        "info",
        "rm note.txt",
        "ls",
        "format"};

    printf("[Step 2] Running scripted commands through the shell parser:\n");
    for (int i = 0; i < 9; ++i)
    {
        printf("summer> %s\n", commands[i]);

        if (strcmp(commands[i], "help") == 0)
        {
            printf("Commands: ls, cat <file>, touch <file>, write <file> <content>, rm <file>, info, format, help\n");
        }
        else if (strcmp(commands[i], "ls") == 0)
        {
            fs->listFiles();
        }
        else if (strcmp(commands[i], "info") == 0)
        {
            fs->showInfo();
        }
        else if (strcmp(commands[i], "format") == 0)
        {
            fs->format(disk);
        }
        else if (strncmp(commands[i], "touch ", 6))
        {
            fs->createFile(commands[i] + 6);
        }
        else if (strncmp(commands[i], "cat ", 4))
        {
            memset(readBuf, 0, sizeof(readBuf));
            int n = fs->readFile(commands[i] + 4, readBuf, sizeof(readBuf) - 1);
            if (n >= 0)
            {
                readBuf[n] = '\0';
                printf("%s\n", readBuf);
            }
        }
        else if (strncmp(commands[i], "rm ", 3))
        {
            fs->deleteFile(commands[i] + 3);
        }
        else if (strncmp(commands[i], "write ", 6))
        {
            const char *rest = commands[i] + 6;
            int split = 0;
            while (rest[split] && rest[split] != ' ')
                split++;

            char name[32];
            for (int j = 0; j < split; ++j)
                name[j] = rest[j];
            name[split] = '\0';

            const char *content = rest + split;
            if (*content == ' ')
                content++;
            fs->writeFile(name, content, strlen(content));
        }
        printf("\n");
    }

    printf("[Step 3] Keyboard IRQ1 and line buffer are installed for make run.\n");
    if (keyboard_read_line(readBuf))
        printf("  Keyboard line available: %s\n", readBuf);

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
