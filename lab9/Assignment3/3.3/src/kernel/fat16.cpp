#include "fat16.h"
#include "stdlib.h"
#include "stdio.h"

FAT16::FAT16()
{
    disk = nullptr;
    mounted = false;
}

// ============================================================
// 辅助函数
// ============================================================

void FAT16::toFAT16Name(const char *name, char *fat16Name)
{
    // 初始化为空格
    for (int i = 0; i < 11; ++i)
        fat16Name[i] = ' ';

    int i = 0, j = 0;

    // 复制文件名部分 (最多8字符)
    while (name[i] && name[i] != '.' && j < 8)
    {
        // 转大写
        char c = name[i];
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        fat16Name[j] = c;
        ++i;
        ++j;
    }

    // 跳过多余的文件名字符
    while (name[i] && name[i] != '.')
        ++i;

    // 复制扩展名部分 (最多3字符)
    if (name[i] == '.')
    {
        ++i;
        j = 8;
        while (name[i] && j < 11)
        {
            char c = name[i];
            if (c >= 'a' && c <= 'z')
                c = c - 'a' + 'A';
            fat16Name[j] = c;
            ++i;
            ++j;
        }
    }
}

void FAT16::fromFAT16Name(const char *fat16Name, char *name)
{
    int j = 0;

    // 复制文件名 (去尾部空格)
    for (int i = 0; i < 8; ++i)
    {
        if (fat16Name[i] != ' ')
        {
            char c = fat16Name[i];
            if (c >= 'A' && c <= 'Z')
                c = c - 'A' + 'a';
            name[j++] = c;
        }
    }

    // 检查扩展名是否为空
    bool hasExt = false;
    for (int i = 8; i < 11; ++i)
    {
        if (fat16Name[i] != ' ')
        {
            hasExt = true;
            break;
        }
    }

    if (hasExt)
    {
        name[j++] = '.';
        for (int i = 8; i < 11; ++i)
        {
            if (fat16Name[i] != ' ')
            {
                char c = fat16Name[i];
                if (c >= 'A' && c <= 'Z')
                    c = c - 'A' + 'a';
                name[j++] = c;
            }
        }
    }

    name[j] = '\0';
}

uint32 FAT16::clusterToSector(uint16 cluster)
{
    // 簇号从 2 开始，簇 2 对应数据区第一个扇区
    return FS_START_SECTOR + DATA_START_OFFSET + (cluster - 2);
}

// ============================================================
// FAT 表操作
// ============================================================

void FAT16::loadFAT()
{
    // FAT 表从 FS_START_SECTOR + 1 开始
    disk->readSectors(FS_START_SECTOR + 1, FAT_SECTORS, (void *)fatTable);
}

void FAT16::syncFAT()
{
    disk->writeSectors(FS_START_SECTOR + 1, FAT_SECTORS, (const void *)fatTable);
}

uint16 FAT16::allocateCluster()
{
    // 从簇 2 开始搜索 (0 和 1 为保留簇)
    for (int i = 2; i < FAT_ENTRIES; ++i)
    {
        if (fatTable[i] == FAT16_FREE)
        {
            fatTable[i] = FAT16_EOF;
            return (uint16)i;
        }
    }
    return 0; // 无空闲簇
}

void FAT16::freeClusterChain(uint16 startCluster)
{
    uint16 current = startCluster;
    while (current >= 2 && current < FAT16_BAD)
    {
        uint16 next = fatTable[current];
        fatTable[current] = FAT16_FREE;
        current = next;
    }
}

// ============================================================
// 目录条目操作
// ============================================================

void FAT16::readDirEntry(int index, DirEntry *entry)
{
    // 根目录区从 FS_START_SECTOR + 1 + FAT_SECTORS 开始
    uint32 rootStart = FS_START_SECTOR + 1 + FAT_SECTORS;
    uint32 sectorIndex = index / ENTRIES_PER_SECTOR;
    uint32 entryOffset = index % ENTRIES_PER_SECTOR;

    uint8 sectorBuf[SECTOR_SIZE];
    disk->readSectors(rootStart + sectorIndex, 1, sectorBuf);
    memcpy(sectorBuf + entryOffset * 32, entry, 32);
}

void FAT16::writeDirEntry(int index, const DirEntry *entry)
{
    uint32 rootStart = FS_START_SECTOR + 1 + FAT_SECTORS;
    uint32 sectorIndex = index / ENTRIES_PER_SECTOR;
    uint32 entryOffset = index % ENTRIES_PER_SECTOR;

    uint8 sectorBuf[SECTOR_SIZE];
    disk->readSectors(rootStart + sectorIndex, 1, sectorBuf);
    memcpy((void *)entry, sectorBuf + entryOffset * 32, 32);
    disk->writeSectors(rootStart + sectorIndex, 1, sectorBuf);
}

int FAT16::findEntry(const char *name)
{
    char fat16Name[11];
    toFAT16Name(name, fat16Name);

    DirEntry entry;
    for (int i = 0; i < MAX_ROOT_ENTRIES; ++i)
    {
        readDirEntry(i, &entry);

        // 空条目 = 目录结束
        if ((uint8)entry.filename[0] == 0x00)
            break;

        // 已删除条目，跳过
        if ((uint8)entry.filename[0] == ATTR_DELETED)
            continue;

        // 比较文件名 (8字节) + 扩展名 (3字节)
        bool match = true;
        for (int j = 0; j < 8; ++j)
        {
            if (entry.filename[j] != fat16Name[j])
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (entry.extension[j] != fat16Name[8 + j])
                {
                    match = false;
                    break;
                }
            }
        }

        if (match)
            return i;
    }

    return -1;
}

int FAT16::findFreeEntry()
{
    DirEntry entry;
    for (int i = 0; i < MAX_ROOT_ENTRIES; ++i)
    {
        readDirEntry(i, &entry);
        if ((uint8)entry.filename[0] == 0x00 ||
            (uint8)entry.filename[0] == ATTR_DELETED)
            return i;
    }
    return -1;
}

// ============================================================
// 文件系统主要接口
// ============================================================

bool FAT16::format(DiskDriver *drv)
{
    this->disk = drv;

    printf("Formatting FAT16 filesystem...\n");

    // 1. 写入超级块
    SuperBlock sb;
    memset(&sb, 0, sizeof(SuperBlock));
    sb.magic[0] = 'S';
    sb.magic[1] = 'F';
    sb.magic[2] = '1';
    sb.magic[3] = '6';
    sb.bytesPerSector = SECTOR_SIZE;
    sb.sectorsPerCluster = 1;
    sb.fatSectors = FAT_SECTORS;
    sb.rootDirSectors = ROOT_DIR_SECTORS;
    sb.maxRootEntries = MAX_ROOT_ENTRIES;
    sb.totalSectors = FAT_ENTRIES - 2; // 可用数据扇区

    disk->writeSectors(FS_START_SECTOR, 1, &sb);

    // 2. 清空 FAT 表
    memset(fatTable, 0, sizeof(fatTable));
    fatTable[0] = 0xFFF8; // 保留
    fatTable[1] = 0xFFFF; // 保留
    syncFAT();

    // 3. 清空根目录区
    uint8 zeroBuf[SECTOR_SIZE];
    memset(zeroBuf, 0, SECTOR_SIZE);
    uint32 rootStart = FS_START_SECTOR + 1 + FAT_SECTORS;
    for (int i = 0; i < ROOT_DIR_SECTORS; ++i)
    {
        disk->writeSectors(rootStart + i, 1, zeroBuf);
    }

    mounted = true;
    printf("Format complete. FAT16 filesystem ready.\n");
    return true;
}

bool FAT16::initialize(DiskDriver *drv)
{
    this->disk = drv;

    // 读取超级块，验证魔数
    SuperBlock sb;
    disk->readSectors(FS_START_SECTOR, 1, &sb);

    if (sb.magic[0] != 'S' || sb.magic[1] != 'F' ||
        sb.magic[2] != '1' || sb.magic[3] != '6')
    {
        printf("No FAT16 filesystem found. Use 'format' to create one.\n");
        return false;
    }

    // 加载 FAT 表到内存
    loadFAT();
    mounted = true;
    printf("FAT16 filesystem mounted.\n");
    return true;
}

bool FAT16::createFile(const char *name)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    // 检查文件是否已存在
    if (findEntry(name) >= 0)
    {
        printf("Error: file '%s' already exists\n", name);
        return false;
    }

    // 找到空闲目录条目
    int idx = findFreeEntry();
    if (idx < 0)
    {
        printf("Error: root directory full\n");
        return false;
    }

    // 创建目录条目
    DirEntry entry;
    memset(&entry, 0, sizeof(DirEntry));

    char fat16Name[11];
    toFAT16Name(name, fat16Name);
    memcpy(fat16Name, entry.filename, 8);
    memcpy(fat16Name + 8, entry.extension, 3);

    entry.attributes = ATTR_ARCHIVE;
    entry.firstCluster = 0; // 空文件没有簇
    entry.fileSize = 0;

    writeDirEntry(idx, &entry);
    return true;
}

bool FAT16::writeFile(const char *name, const char *data, uint32 size)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    int idx = findEntry(name);
    DirEntry entry;

    if (idx < 0)
    {
        // 文件不存在，先创建
        idx = findFreeEntry();
        if (idx < 0)
        {
            printf("Error: root directory full\n");
            return false;
        }
        memset(&entry, 0, sizeof(DirEntry));
        char fat16Name[11];
        toFAT16Name(name, fat16Name);
        memcpy(fat16Name, entry.filename, 8);
        memcpy(fat16Name + 8, entry.extension, 3);
        entry.attributes = ATTR_ARCHIVE;
    }
    else
    {
        readDirEntry(idx, &entry);
        // 释放旧的簇链
        if (entry.firstCluster >= 2)
        {
            freeClusterChain(entry.firstCluster);
        }
    }

    if (size == 0)
    {
        entry.firstCluster = 0;
        entry.fileSize = 0;
        writeDirEntry(idx, &entry);
        syncFAT();
        return true;
    }

    // 计算需要的簇数
    uint32 clustersNeeded = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

    // 分配簇链
    uint16 firstCluster = 0;
    uint16 prevCluster = 0;

    for (uint32 i = 0; i < clustersNeeded; ++i)
    {
        uint16 cluster = allocateCluster();
        if (cluster == 0)
        {
            printf("Error: disk full\n");
            if (firstCluster)
                freeClusterChain(firstCluster);
            syncFAT();
            return false;
        }

        if (i == 0)
            firstCluster = cluster;
        else
            fatTable[prevCluster] = cluster;

        // 将数据写入该簇对应的扇区
        uint8 sectorBuf[SECTOR_SIZE];
        memset(sectorBuf, 0, SECTOR_SIZE);
        uint32 copySize = size - i * SECTOR_SIZE;
        if (copySize > SECTOR_SIZE)
            copySize = SECTOR_SIZE;
        memcpy((void *)(data + i * SECTOR_SIZE), sectorBuf, copySize);
        disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);

        prevCluster = cluster;
    }

    // 更新目录条目
    entry.firstCluster = firstCluster;
    entry.fileSize = size;
    writeDirEntry(idx, &entry);
    syncFAT();

    return true;
}

int FAT16::readFile(const char *name, char *buffer, uint32 maxSize)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return -1;
    }

    int idx = findEntry(name);
    if (idx < 0)
    {
        printf("Error: file '%s' not found\n", name);
        return -1;
    }

    DirEntry entry;
    readDirEntry(idx, &entry);

    if (entry.fileSize == 0)
        return 0;

    uint32 readSize = entry.fileSize;
    if (readSize > maxSize)
        readSize = maxSize;

    uint16 cluster = entry.firstCluster;
    uint32 bytesRead = 0;
    uint8 sectorBuf[SECTOR_SIZE];

    while (cluster >= 2 && cluster < FAT16_BAD && bytesRead < readSize)
    {
        disk->readSectors(clusterToSector(cluster), 1, sectorBuf);

        uint32 copySize = readSize - bytesRead;
        if (copySize > SECTOR_SIZE)
            copySize = SECTOR_SIZE;

        memcpy(sectorBuf, buffer + bytesRead, copySize);
        bytesRead += copySize;

        cluster = fatTable[cluster];
    }

    return (int)bytesRead;
}

bool FAT16::deleteFile(const char *name)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    int idx = findEntry(name);
    if (idx < 0)
    {
        printf("Error: file '%s' not found\n", name);
        return false;
    }

    DirEntry entry;
    readDirEntry(idx, &entry);

    // 释放簇链
    if (entry.firstCluster >= 2)
    {
        freeClusterChain(entry.firstCluster);
        syncFAT();
    }

    // 标记目录条目为已删除
    entry.filename[0] = (char)ATTR_DELETED;
    writeDirEntry(idx, &entry);

    return true;
}

/**
 * renameFile - 修改文件的 8.3 名称，不触碰文件数据
 *
 * 重命名是一种纯元数据操作，只修改 DirEntry 中的 filename[8] 和 extension[3]，
 * 以下字段完全不变：
 *   - firstCluster  → FAT 链不动，数据在原处
 *   - fileSize      → 内容未变，大小不变
 *   - attributes    → 属性不变
 *
 * 因此无需 syncFAT()、无需 allocateCluster()、无需 freeClusterChain()。
 *
 * 双重校验：
 *   1) 旧文件名必须存在（否则无目录项可改）
 *   2) 新文件名不能与已有文件冲突（否则两个目录项指向同名字）
 *
 * writeDirEntry(oldIdx, &entry) 做了什么：
 *   - 根据 oldIdx 计算出根目录区中的扇区号和扇区内偏移
 *   - 用"读-改-写"策略：读出整个扇区 → 把 32 字节 entry 覆盖到对应偏移 → 写回扇区
 *   - 旧名称由此被新名称替换，文件数据毫发无伤
 */
bool FAT16::renameFile(const char *oldName, const char *newName)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    // ================================================================
    // 校验1：旧文件必须存在，否则没有目录项可以改名
    // ================================================================
    int oldIdx = findEntry(oldName);
    if (oldIdx < 0)
    {
        printf("Error: file '%s' not found\n", oldName);
        return false;
    }

    // ================================================================
    // 校验2：新文件名不能与已有文件冲突
    // 若允许重命名到已存在的名字，会导致两个目录项有相同的 8.3 名称，
    // 后续 findEntry 会永远只匹配第一个（无法区分），造成逻辑混乱
    // ================================================================
    if (findEntry(newName) >= 0)
    {
        printf("Error: file '%s' already exists\n", newName);
        return false;
    }

    // ================================================================
    // 读出旧目录项的全部 32 字节
    // entry 结构：[filename:8][extension:3][attributes:1][reserved:14][firstCluster:2][fileSize:4]
    // 当前内容示例：
    //   filename="OLD     ", extension="TXT", firstCluster=2, fileSize=36
    // ================================================================
    DirEntry entry;
    readDirEntry(oldIdx, &entry);

    // ================================================================
    // 转换新名字为 FAT16 8.3 格式，然后改写 entry 的名称字段
    // 例："renamed.txt" → fat16Name = "RENAMED TXT"
    //
    // memcpy(fat16Name,         entry.filename,  8) → 将 "RENAMED " 写入 filename[0..7]
    // memcpy(fat16Name + 8,     entry.extension, 3) → 将 "TXT"    写入 extension[0..2]
    //
    // 改写后 entry：
    //   filename="RENAMED ", extension="TXT", firstCluster=2, fileSize=36
    //             ↑↑↑ 只改了这里        ↑↑↑ 只改了这里     其余字段保持原值
    // ================================================================
    char fat16Name[11];
    toFAT16Name(newName, fat16Name);
    memcpy(fat16Name, entry.filename, 8);     // src=fat16Name → dst=entry.filename (8字节)
    memcpy(fat16Name + 8, entry.extension, 3); // src=fat16Name+8 → dst=entry.extension (3字节)

    // ================================================================
    // 将修改后的 entry 写回磁盘根目录区 (旧条目的同一位置)
    //
    // writeDirEntry(oldIdx, &entry) 内部执行：
    //   1. 计算: sectorIdx       = oldIdx / 16   (每扇区 16 个条目)
    //            entryOffset     = oldIdx % 16   (扇区内偏移)
    //            rootSectorStart = FS_START(500) + 1 + FAT_SECTORS(32) = 533
    //   2. disk->readSectors(533 + sectorIdx, 1, sectorBuf)   → 读出一整个扇区
    //   3. memcpy(entry → sectorBuf + entryOffset*32, 32)     → 把新 entry 写入扇区缓冲的正确位置
    //   4. disk->writeSectors(533 + sectorIdx, 1, sectorBuf)  → 扇区整体写回磁盘
    //
    // 为什么是"读-改-写"？因为一个扇区存 16 个 DirEntry，
    // 不能只写 32 字节——磁盘 I/O 最小单位是扇区(512字节)，
    // 必须读出整扇区 → 修改其中 32 字节 → 整扇区写回，才不会破坏同扇区其他条目
    // ================================================================
    writeDirEntry(oldIdx, &entry);

    return true;
}

void FAT16::listFiles()
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return;
    }

    printf("FILENAME       SIZE      CLUSTER\n");
    printf("-------------------------------\n");

    int count = 0;
    DirEntry entry;
    for (int i = 0; i < MAX_ROOT_ENTRIES; ++i)
    {
        readDirEntry(i, &entry);

        if ((uint8)entry.filename[0] == 0x00)
            break;
        if ((uint8)entry.filename[0] == ATTR_DELETED)
            continue;

        char name[16];
        fromFAT16Name(entry.filename, name);
        printf("%s  %d  %d\n", name, entry.fileSize, entry.firstCluster);
        count++;
    }

    printf("-------------------------------\n");
    printf("Total: %d file(s)\n", count);
}

void FAT16::showInfo()
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return;
    }

    int usedClusters = 0;
    int freeClusters = 0;
    for (int i = 2; i < FAT_ENTRIES; ++i)
    {
        if (fatTable[i] == FAT16_FREE)
            freeClusters++;
        else
            usedClusters++;
    }

    printf("=== FAT16 Filesystem Info ===\n");
    printf("  Start sector  : %d\n", FS_START_SECTOR);
    printf("  Cluster size  : %d bytes\n", SECTOR_SIZE);
    printf("  Total clusters: %d\n", FAT_ENTRIES - 2);
    printf("  Used clusters : %d (%d bytes)\n", usedClusters, usedClusters * SECTOR_SIZE);
    printf("  Free clusters : %d (%d bytes)\n", freeClusters, freeClusters * SECTOR_SIZE);
    printf("  Max files     : %d\n", MAX_ROOT_ENTRIES);
}
