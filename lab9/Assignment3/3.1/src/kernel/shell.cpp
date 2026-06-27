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

/**
 * runDemo - 测试 appendFile 的三种核心场景
 *
 * 测试通过 3 次追加操作，覆盖了 appendFile 的所有分支：
 *   场景1（Step 2）：文件不存在时追加 -> 走 writeFile 路径创建新文件
 *   场景2（Step 3）：追加数据超过一簇 -> 触发尾簇填满 + 新簇分配 + FAT 链扩展
 *   场景3（Step 4）：追加短线数据 -> 直接填入尾簇剩余空间，不分配新簇
 *
 * 数据布局演变（每簇 512 字节）：
 *   初始：  (空磁盘)
 *   Step 2: [HEAD-__________...]  fileSize=5,   簇0 只有前 5 字节有效
 *   Step 3: [HEAD-AAAAAA...AAA]  fileSize=625,  簇0 满，簇1 有 113 字节（625-512=113）
 *   Step 4: [HEAD-AAAA...TAIL]   fileSize=630,  直接在簇1 偏移 113 处写入，无需新簇
 *                                          ^^^^
 *  最终：  HEAD- + 620个字母 + -TAIL = 630 字节，占用 2 簇
 */
void Shell::runDemo()
{
    char readBuf[1024];
    char largeBlock[621];
    // 构造 620 字节的大数据块（ABCDEF...XYZ 循环），
    // 与前面的 "HEAD-"(5字节) 合计 625 字节 > 512，一定会跨越簇边界
    for (int i = 0; i < 620; ++i)
    {
        largeBlock[i] = 'A' + (i % 26);
    }
    largeBlock[620] = '\0';

    printf("\n==============================================\n");
    printf("      Assignment3.1 appendFile Test\n");
    printf("==============================================\n\n");

    /**
     * Step 1: 格式化文件系统
     * 写入超级块 -> 清空 FAT 表（保留簇0/1）-> 清空根目录区
     * 此时磁盘上没有任何文件，FAT 表中所有数据簇均为 FAT16_FREE(0x0000)
     */
    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    /**
     * Step 2: 对不存在的文件追加 "HEAD-"（5 字节）
     * appendFile 内部发现文件不存在（findEntry 返回 -1），
     * 走"情况1"分支 -> 委托 writeFile 创建文件并写入 5 字节到簇2
     * 此时 fileSize=5, firstCluster=2, FAT[2]=FAT16_EOF
     */
    printf("[Step 2] appendFile on a missing file...\n");
    const char *head = "HEAD-";
    fs->appendFile("append.txt", head, strlen(head));
    printf("  append missing file: append.txt += %d bytes\n", strlen(head));

    /**
     * Step 3: 追加 largeBlock（620 字节）
     * 文件已有 5 字节 + 追加 620 = 625 字节 > 512（一簇），触发跨簇：
     *   - 尾簇（簇2）：fileSize=5, tailOffset=5, tailSpace=507，
     *     用"读-改-写"填入 largeBlock 的前 507 字节
     *   - 还需要 620-507=113 字节，分配簇3，链接 FAT[2]=3, FAT[3]=EOF
     *   - 将剩余 113 字节写入簇3
     *   - fileSize 更新为 625
     */
    printf("[Step 3] append a 620-byte block, crossing one cluster...\n");
    fs->appendFile("append.txt", largeBlock, strlen(largeBlock));
    printf("  append existing file: append.txt += %d bytes\n", strlen(largeBlock));

    /**
     * Step 4: 追加 "-TAIL"（5 字节）
     * 当前 fileSize=625, 尾簇（簇3）有 625-512=113 字节已用，tailSpace=512-113=399
     * 5 字节 < 399，所以不需要新簇：
     *   - 读出簇3所在扇区
     *   - 从偏移 113 处写入 "-TAIL"
     *   - fileSize 更新为 625+5=630
     *   - 不分配新簇，仍然是 2 个簇
     */
    printf("[Step 4] append a tail segment into the last cluster...\n");
    const char *tail = "-TAIL";
    fs->appendFile("append.txt", tail, strlen(tail));
    printf("  append existing file: append.txt += %d bytes\n", strlen(tail));
    printf("\n");

    /**
     * Step 5: 验证目录条目
     * 预期输出：append.txt  size=630  firstCluster=2
     */
    printf("[Step 5] Listing files after appends:\n");
    fs->listFiles();
    printf("\n");

    /**
     * Step 6: 读取并打印文件内容，验证追加写入的数据完整性
     * readFile 沿 FAT 链（2 -> 3 -> EOF）逐簇读出，组装成完整内容
     * 预期输出：HEAD-AAAA...ZZZZ-TAIL 共 630 字符
     */
    printf("[Step 6] Reading final content:\n");
    memset(readBuf, 0, sizeof(readBuf));
    int n = fs->readFile("append.txt", readBuf, sizeof(readBuf) - 1);
    if (n > 0)
    {
        readBuf[n] = '\0';
        printf("  append.txt final size: %d bytes\n", n);
        printf("  append.txt content: %s\n", readBuf);
    }
    printf("\n");

    /**
     * Step 7: 显示文件系统统计信息
     * 验证 used clusters = 2（簇2 和簇3），free = 8188
     */
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
