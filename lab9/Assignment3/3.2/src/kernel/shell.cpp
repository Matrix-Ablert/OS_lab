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
 * runDemo - 测试 getFileClusterCount 与 getFragmentation
 *
 * 测试通过"创建→查看→删除→再创建"的流程，展示碎片率的动态变化：
 *
 *   阶段1（Step 2-3）：连续分配，碎片率 0%
 *     连续创建 3 个文件，FAT 表按顺序分配簇，每个文件内部完全连续。
 *
 *   阶段2（Step 5）：删除中间文件制造空洞
 *     删除 gap.bin 释放簇5,6，在磁盘中间留下空洞。
 *
 *   阶段3（Step 6-8）：新文件跨空洞分配，碎片率上升
 *     写入 new.bin(4簇)，allocateCluster 先填满空洞(5,6)，
 *     再跳到 tail.bin 之后(9,10)，形成 5→6→9→10，
 *     其中 6→9 不连续，产生 1 个碎片链接。
 *
 * FAT 链演变示意：
 *   阶段1:  [2,3,4] [5,6] [7,8]
 *           first    gap   tail
 *   阶段2:  [2,3,4] [空洞] [7,8]
 *           first          tail
 *   阶段3:  [2,3,4] [5,6] [7,8] [9,10]
 *           first    new\__/tail \new/
 *                          ↑
 *                     6→9 不连续!
 */
void Shell::runDemo()
{
    char data[2048];

    printf("\n==============================================\n");
    printf("      Assignment3.2 Space and Fragmentation Test\n");
    printf("==============================================\n\n");

    // ============================================================
    // Step 1: 格式化，清空所有数据和 FAT 表
    // ============================================================
    printf("[Step 1] Formatting file system...\n");
    fs->format(disk);
    printf("\n");

    // ============================================================
    // Step 2: 创建 3 个连续分配的文件
    //   first.bin: 1536 bytes = ceil(1536/512) = 3 簇 → 簇2,3,4
    //   gap.bin:   1024 bytes = ceil(1024/512) = 2 簇 → 簇5,6
    //   tail.bin:  1024 bytes = 2 簇 → 簇7,8
    // allocateCluster 从簇2开始线性搜索空闲簇，所以分配是连续的
    // ============================================================
    printf("[Step 2] Creating contiguous multi-cluster files...\n");
    for (int i = 0; i < 1536; ++i)
        data[i] = 'A';
    fs->writeFile("first.bin", data, 1536);
    printf("  first.bin size 1536 bytes\n");

    for (int i = 0; i < 1024; ++i)
        data[i] = 'B';
    fs->writeFile("gap.bin", data, 1024);
    printf("  gap.bin size 1024 bytes\n");

    for (int i = 0; i < 1024; ++i)
        data[i] = 'C';
    fs->writeFile("tail.bin", data, 1024);
    printf("  tail.bin size 1024 bytes\n");
    printf("\n");

    // ============================================================
    // Step 3: 阶段1 — 碎片率应为 0%
    //   每个文件内部的簇号都是递增+1的连续序列，没有任何跳跃
    // ============================================================
    printf("[Step 3] Cluster counts before fragmentation:\n");
    printf("  first.bin clusters: %d\n", fs->getFileClusterCount("first.bin"));
    printf("  gap.bin clusters  : %d\n", fs->getFileClusterCount("gap.bin"));
    printf("  tail.bin clusters : %d\n", fs->getFileClusterCount("tail.bin"));
    printf("  fragmentation     : %d%%\n", (int)(fs->getFragmentation() * 100));
    printf("\n");

    // ============================================================
    // Step 4: 列出所有文件，观察起始簇号
    //   预期：first.bin@2, gap.bin@5, tail.bin@7
    // ============================================================
    printf("[Step 4] Listing files before delete:\n");
    fs->listFiles();
    printf("\n");

    // ============================================================
    // Step 5: 删除 gap.bin → freeClusterChain 释放簇5,6
    //   磁盘中间形成一个空洞（第一次分配策略下，空洞恰在中间）
    // ============================================================
    printf("[Step 5] Deleting gap.bin to create a free hole...\n");
    fs->deleteFile("gap.bin");
    printf("  Deleted: gap.bin\n");
    printf("\n");

    // ============================================================
    // Step 6: 创建 new.bin（2048 bytes = 4 簇），跨空洞分配
    //   allocateCluster 从簇2开始搜索：
    //     找到簇5(空闲) → 分配
    //     找到簇6(空闲) → 分配
    //     簇7,8 被 tail.bin 占用 → 跳过
    //     找到簇9(空闲) → 分配
    //     找到簇10(空闲) → 分配
    //   FAT 链: 5 → 6 → 9 → 10 → EOF
    //   其中 6→9 是不连续的（6+1=7≠9）→ 1 个碎片链接
    // ============================================================
    printf("[Step 6] Creating new.bin across the hole and later space...\n");
    for (int i = 0; i < 2048; ++i)
        data[i] = 'N';
    fs->writeFile("new.bin", data, 2048);
    printf("  new.bin size 2048 bytes\n");
    printf("\n");

    // ============================================================
    // Step 7: 再次列出文件
    //   预期：first.bin@2, new.bin@5, tail.bin@7
    //   new.bin 复用了 gap.bin 释放的目录条目
    // ============================================================
    printf("[Step 7] Listing files after fragmented allocation:\n");
    fs->listFiles();
    printf("\n");

    // ============================================================
    // Step 8: 阶段3 — 碎片率验证
    //   first.bin: 2→3→4    (2个链接, 0碎片)
    //   tail.bin:  7→8       (1个链接, 0碎片)
    //   new.bin:   5→6→9→10 (3个链接, 1碎片: 6→9)
    //   总计: 6个链接, 1个碎片 → 1/6 ≈ 16%
    // ============================================================
    printf("[Step 8] Cluster counts and fragmentation after new.bin:\n");
    printf("  first.bin clusters: %d\n", fs->getFileClusterCount("first.bin"));
    printf("  tail.bin clusters : %d\n", fs->getFileClusterCount("tail.bin"));
    printf("  new.bin clusters  : %d\n", fs->getFileClusterCount("new.bin"));
    printf("  fragmentation     : %d%%\n", (int)(fs->getFragmentation() * 100));
    printf("\n");

    // ============================================================
    // Step 9: 最终统计
    //   used = 3(first) + 2(tail) + 4(new) = 9 簇
    //   free = 8190 - 9 = 8181
    // ============================================================
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
