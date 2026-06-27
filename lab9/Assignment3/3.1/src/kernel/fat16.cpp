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

/**
 * appendFile - 向指定文件尾部追加数据
 *
 * 三种情况：
 *   1) 文件不存在       -> 委托 writeFile() 创建文件并写入（等同于覆盖写）
 *   2) 文件存在但为空    -> 委托 writeFile() 初始化第一条簇链
 *   3) 文件存在且有数据   -> 以下 6 步完成追加
 *
 * 核心流程（情况3）：
 *   Step A: 沿 FAT 链找到最后一个簇
 *   Step B: 计算尾簇剩余空间，预检磁盘空闲簇是否足够
 *   Step C: 如果尾簇未满（fileSize % 512 != 0），用"读-改-写"策略填满它
 *   Step D: 剩余数据逐簇分配新簇，串联到 FAT 链末尾
 *   Step E: 累加 fileSize，写回目录条目
 *   Step F: 将内存中的 FAT 表同步到磁盘
 */
bool FAT16::appendFile(const char *name, const char *data, uint32 size)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    // ============================
    // 情况1：文件不存在 -> 委托 writeFile 创建并写入
    // writeFile 内部会完成：找空闲目录项 -> 分配簇链 -> 写数据 -> 更新元数据
    // ============================
    int idx = findEntry(name);
    if (idx < 0)
    {
        return writeFile(name, data, size);
    }

    // 空数据直接返回，无需任何操作
    if (size == 0)
    {
        return true;
    }

    DirEntry entry;
    readDirEntry(idx, &entry);

    // ============================
    // 情况2：文件存在但 firstCluster 无效或 fileSize==0
    // 说明文件没有实际数据簇，走 writeFile 分配初始簇链
    // （firstCluster < 2 即 0 或 1，这两个是 FAT 保留项）
    // ============================
    if (entry.firstCluster < 2 || entry.fileSize == 0)
    {
        return writeFile(name, data, size);
    }

    // ============================
    // 情况3：文件存在且有数据 -> 执行追加逻辑
    // ============================

    // --- Step A: 沿 FAT 链遍历到最后一个簇 ---
    // FAT 链结构：fatTable[clusterA] = clusterB, fatTable[clusterB] = clusterC, ...
    // 终止条件：fatTable[X] == FAT16_EOF (0xFFF8)，此时 X 就是链尾簇
    uint16 lastCluster = entry.firstCluster;
    while (fatTable[lastCluster] >= 2 && fatTable[lastCluster] < FAT16_BAD)
    {
        lastCluster = fatTable[lastCluster];
    }

    // --- Step B: 计算尾簇剩余空间 & 预检磁盘容量 ---
    // 例：fileSize=100 时，tailOffset=100, tailSpace=412 (512-100)
    //     表示尾簇从偏移 100 处开始还有 412 字节可写
    // 例：fileSize=512 时，tailOffset=0,  tailSpace=0（刚好写满，需直接分配新簇）
    uint32 tailOffset = entry.fileSize % SECTOR_SIZE;   // 尾簇内已用字节数（0~511）
    uint32 tailSpace  = tailOffset == 0 ? 0 : SECTOR_SIZE - tailOffset;  // 尾簇剩余可写字节数
    uint32 bytesAfterTail = size;
    if (tailSpace > bytesAfterTail)
        bytesAfterTail = 0;          // 尾簇空间够用，不需要新簇
    else
        bytesAfterTail -= tailSpace; // 扣掉尾簇填满的，剩下的需要新簇

    uint32 newClustersNeeded = (bytesAfterTail + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint32 freeClusters = 0;
    // 预检空闲簇数：遍历一遍 FAT 表，确保原子性——不会"写到一半才发现磁盘满了"
    for (int i = 2; i < FAT_ENTRIES; ++i)
    {
        if (fatTable[i] == FAT16_FREE)
            freeClusters++;
    }
    if (freeClusters < newClustersNeeded)
    {
        printf("Error: disk full\n");
        return false;
    }

    // --- Step C: 如果尾簇未满，用"读-改-写"策略填入数据 ---
    // 为什么是"读-改-写"？因为不能直接覆盖扇区——扇区前半部分存着原有数据，
    // 必须先把整个扇区读出来，在新数据写到正确偏移后，再整体写回
    uint32 bytesWritten = 0;
    if (tailOffset != 0 && bytesWritten < size)
    {
        uint8 sectorBuf[SECTOR_SIZE];
        disk->readSectors(clusterToSector(lastCluster), 1, sectorBuf);  // 读出原扇区

        uint32 copySize = size;
        if (copySize > tailSpace)
            copySize = tailSpace;    // 不超过尾簇剩余空间

        memcpy((void *)data, sectorBuf + tailOffset, copySize);        // 从偏移处填入新数据
        disk->writeSectors(clusterToSector(lastCluster), 1, sectorBuf); // 写回磁盘
        bytesWritten += copySize;
    }

    // --- Step D: 剩余数据超出尾簇容量，逐簇分配 & 链接到 FAT 链尾 ---
    // prevCluster 初始指向原链尾；每分配一个新簇，就把 FAT[prevCluster] 指向新簇，
    // 然后 prevCluster 后移，这样新簇就被串联到了 FAT 链末尾
    uint16 prevCluster = lastCluster;
    while (bytesWritten < size)
    {
        uint16 cluster = allocateCluster();       // 从 FAT 表找空闲簇，标记为 FAT16_EOF
        fatTable[prevCluster] = cluster;           // 将新簇挂到链尾

        uint8 sectorBuf[SECTOR_SIZE];
        memset(sectorBuf, 0, SECTOR_SIZE);         // 新簇清零

        uint32 copySize = size - bytesWritten;
        if (copySize > SECTOR_SIZE)
            copySize = SECTOR_SIZE;                // 一簇最多 512 字节

        memcpy((void *)(data + bytesWritten), sectorBuf, copySize);
        disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);

        prevCluster = cluster;                     // 指针后移，下一轮挂在这个新簇后面
        bytesWritten += copySize;
    }

    // --- Step E: 更新目录条目中的文件大小 ---
    // 注意：是累加 (+=)，而非覆盖 (=)，这是与 writeFile 的核心区别
    entry.fileSize += size;
    writeDirEntry(idx, &entry);

    // --- Step F: 同步 FAT 表到磁盘 ---
    // FAT 链改动只在内存中（fatTable[]），必须写回磁盘的 32 个 FAT 扇区，
    // 否则下次挂载时 FAT 还是旧的，新追加的数据将不可见甚至损坏文件系统
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
