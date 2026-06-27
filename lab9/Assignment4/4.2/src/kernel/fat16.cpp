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

bool FAT16::splitPath(const char *path, char *dirName, char *baseName)
{
    int slash = -1;
    for (int i = 0; path[i]; ++i)
    {
        if (path[i] == '/')
        {
            slash = i;
            break;
        }
    }

    if (slash < 0)
        return false;

    for (int i = 0; i < slash; ++i)
        dirName[i] = path[i];
    dirName[slash] = '\0';
    strcpy(path + slash + 1, baseName);
    return true;
}

void FAT16::readDirEntryInDir(uint16 dirCluster, int index, DirEntry *entry)
{
    uint8 sectorBuf[SECTOR_SIZE];
    disk->readSectors(clusterToSector(dirCluster), 1, sectorBuf);
    memcpy(sectorBuf + index * 32, entry, 32);
}

void FAT16::writeDirEntryInDir(uint16 dirCluster, int index, const DirEntry *entry)
{
    uint8 sectorBuf[SECTOR_SIZE];
    disk->readSectors(clusterToSector(dirCluster), 1, sectorBuf);
    memcpy((void *)entry, sectorBuf + index * 32, 32);
    disk->writeSectors(clusterToSector(dirCluster), 1, sectorBuf);
}

int FAT16::findEntryInDir(uint16 dirCluster, const char *name)
{
    char fat16Name[11];
    toFAT16Name(name, fat16Name);

    DirEntry entry;
    for (int i = 0; i < ENTRIES_PER_SECTOR; ++i)
    {
        readDirEntryInDir(dirCluster, i, &entry);
        if ((uint8)entry.filename[0] == 0x00)
            break;
        if ((uint8)entry.filename[0] == ATTR_DELETED)
            continue;

        bool match = true;
        for (int j = 0; j < 8; ++j)
        {
            if (entry.filename[j] != fat16Name[j])
                match = false;
        }
        for (int j = 0; j < 3; ++j)
        {
            if (entry.extension[j] != fat16Name[8 + j])
                match = false;
        }
        if (match)
            return i;
    }
    return -1;
}

int FAT16::findFreeEntryInDir(uint16 dirCluster)
{
    DirEntry entry;
    for (int i = 0; i < ENTRIES_PER_SECTOR; ++i)
    {
        readDirEntryInDir(dirCluster, i, &entry);
        if ((uint8)entry.filename[0] == 0x00 ||
            (uint8)entry.filename[0] == ATTR_DELETED)
            return i;
    }
    return -1;
}

bool FAT16::isDirEmpty(uint16 dirCluster)
{
    DirEntry entry;
    for (int i = 2; i < ENTRIES_PER_SECTOR; ++i)
    {
        readDirEntryInDir(dirCluster, i, &entry);
        if ((uint8)entry.filename[0] == 0x00)
            return true;
        if ((uint8)entry.filename[0] != ATTR_DELETED)
            return false;
    }
    return true;
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

    char dirName[32], baseName[32];
    if (splitPath(name, dirName, baseName))
    {
        int dirIdx = findEntry(dirName);
        if (dirIdx < 0)
        {
            printf("Error: directory '%s' not found\n", dirName);
            return false;
        }
        DirEntry dirEntry;
        readDirEntry(dirIdx, &dirEntry);
        if (!(dirEntry.attributes & ATTR_DIRECTORY))
        {
            printf("Error: '%s' is not a directory\n", dirName);
            return false;
        }
        if (findEntryInDir(dirEntry.firstCluster, baseName) >= 0)
        {
            printf("Error: file '%s' already exists\n", name);
            return false;
        }
        int childIdx = findFreeEntryInDir(dirEntry.firstCluster);
        if (childIdx < 0)
        {
            printf("Error: directory full\n");
            return false;
        }

        DirEntry entry;
        memset(&entry, 0, sizeof(DirEntry));
        char fat16Name[11];
        toFAT16Name(baseName, fat16Name);
        memcpy(fat16Name, entry.filename, 8);
        memcpy(fat16Name + 8, entry.extension, 3);
        entry.attributes = ATTR_ARCHIVE;
        writeDirEntryInDir(dirEntry.firstCluster, childIdx, &entry);
        return true;
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

    char dirName[32], baseName[32];
    if (splitPath(name, dirName, baseName))
    {
        int dirIdx = findEntry(dirName);
        if (dirIdx < 0)
        {
            printf("Error: directory '%s' not found\n", dirName);
            return false;
        }
        DirEntry dirEntry;
        readDirEntry(dirIdx, &dirEntry);
        if (!(dirEntry.attributes & ATTR_DIRECTORY))
        {
            printf("Error: '%s' is not a directory\n", dirName);
            return false;
        }

        int idx = findEntryInDir(dirEntry.firstCluster, baseName);
        DirEntry entry;
        if (idx < 0)
        {
            idx = findFreeEntryInDir(dirEntry.firstCluster);
            if (idx < 0)
            {
                printf("Error: directory full\n");
                return false;
            }
            memset(&entry, 0, sizeof(DirEntry));
            char fat16Name[11];
            toFAT16Name(baseName, fat16Name);
            memcpy(fat16Name, entry.filename, 8);
            memcpy(fat16Name + 8, entry.extension, 3);
            entry.attributes = ATTR_ARCHIVE;
        }
        else
        {
            readDirEntryInDir(dirEntry.firstCluster, idx, &entry);
            if (entry.firstCluster >= 2)
                freeClusterChain(entry.firstCluster);
        }

        if (size == 0)
        {
            entry.firstCluster = 0;
            entry.fileSize = 0;
            writeDirEntryInDir(dirEntry.firstCluster, idx, &entry);
            syncFAT();
            return true;
        }

        uint32 clustersNeeded = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
        uint16 firstCluster = 0, prevCluster = 0;
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

            uint8 sectorBuf[SECTOR_SIZE];
            memset(sectorBuf, 0, SECTOR_SIZE);
            uint32 copySize = size - i * SECTOR_SIZE;
            if (copySize > SECTOR_SIZE)
                copySize = SECTOR_SIZE;
            memcpy((void *)(data + i * SECTOR_SIZE), sectorBuf, copySize);
            disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);
            prevCluster = cluster;
        }

        entry.firstCluster = firstCluster;
        entry.fileSize = size;
        writeDirEntryInDir(dirEntry.firstCluster, idx, &entry);
        syncFAT();
        return true;
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

    char dirName[32], baseName[32];
    if (splitPath(name, dirName, baseName))
    {
        int dirIdx = findEntry(dirName);
        if (dirIdx < 0)
        {
            printf("Error: directory '%s' not found\n", dirName);
            return -1;
        }
        DirEntry dirEntry;
        readDirEntry(dirIdx, &dirEntry);
        int childIdx = findEntryInDir(dirEntry.firstCluster, baseName);
        if (childIdx < 0)
        {
            printf("Error: file '%s' not found\n", name);
            return -1;
        }

        DirEntry entry;
        readDirEntryInDir(dirEntry.firstCluster, childIdx, &entry);
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

    char dirName[32], baseName[32];
    if (splitPath(name, dirName, baseName))
    {
        int dirIdx = findEntry(dirName);
        if (dirIdx < 0)
        {
            printf("Error: directory '%s' not found\n", dirName);
            return false;
        }
        DirEntry dirEntry;
        readDirEntry(dirIdx, &dirEntry);
        int childIdx = findEntryInDir(dirEntry.firstCluster, baseName);
        if (childIdx < 0)
        {
            printf("Error: file '%s' not found\n", name);
            return false;
        }
        DirEntry entry;
        readDirEntryInDir(dirEntry.firstCluster, childIdx, &entry);
        if (entry.firstCluster >= 2)
        {
            freeClusterChain(entry.firstCluster);
            syncFAT();
        }
        entry.filename[0] = (char)ATTR_DELETED;
        writeDirEntryInDir(dirEntry.firstCluster, childIdx, &entry);
        return true;
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

bool FAT16::mkdir(const char *path)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }
    if (findEntry(path) >= 0)
    {
        printf("Error: file '%s' already exists\n", path);
        return false;
    }

    int idx = findFreeEntry();
    if (idx < 0)
    {
        printf("Error: root directory full\n");
        return false;
    }

    uint16 cluster = allocateCluster();
    if (cluster == 0)
    {
        printf("Error: disk full\n");
        return false;
    }

    uint8 sectorBuf[SECTOR_SIZE];
    memset(sectorBuf, 0, SECTOR_SIZE);

    DirEntry dot;
    memset(&dot, 0, sizeof(DirEntry));
    dot.filename[0] = '.';
    for (int i = 1; i < 8; ++i)
        dot.filename[i] = ' ';
    for (int i = 0; i < 3; ++i)
        dot.extension[i] = ' ';
    dot.attributes = ATTR_DIRECTORY;
    dot.firstCluster = cluster;
    memcpy(&dot, sectorBuf, 32);

    DirEntry dotdot;
    memset(&dotdot, 0, sizeof(DirEntry));
    dotdot.filename[0] = '.';
    dotdot.filename[1] = '.';
    for (int i = 2; i < 8; ++i)
        dotdot.filename[i] = ' ';
    for (int i = 0; i < 3; ++i)
        dotdot.extension[i] = ' ';
    dotdot.attributes = ATTR_DIRECTORY;
    dotdot.firstCluster = 0;
    memcpy(&dotdot, sectorBuf + 32, 32);
    disk->writeSectors(clusterToSector(cluster), 1, sectorBuf);

    DirEntry entry;
    memset(&entry, 0, sizeof(DirEntry));
    char fat16Name[11];
    toFAT16Name(path, fat16Name);
    memcpy(fat16Name, entry.filename, 8);
    memcpy(fat16Name + 8, entry.extension, 3);
    entry.attributes = ATTR_DIRECTORY;
    entry.firstCluster = cluster;
    entry.fileSize = 0;
    writeDirEntry(idx, &entry);
    syncFAT();
    return true;
}

bool FAT16::rmdir(const char *path)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    int idx = findEntry(path);
    if (idx < 0)
    {
        printf("Error: directory '%s' not found\n", path);
        return false;
    }

    DirEntry entry;
    readDirEntry(idx, &entry);
    if (!(entry.attributes & ATTR_DIRECTORY))
    {
        printf("Error: '%s' is not a directory\n", path);
        return false;
    }
    if (!isDirEmpty(entry.firstCluster))
    {
        printf("Error: directory '%s' not empty\n", path);
        return false;
    }

    freeClusterChain(entry.firstCluster);
    syncFAT();
    entry.filename[0] = (char)ATTR_DELETED;
    writeDirEntry(idx, &entry);
    return true;
}

bool FAT16::renameFile(const char *oldName, const char *newName)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return false;
    }

    int oldIdx = findEntry(oldName);
    if (oldIdx < 0)
    {
        printf("Error: file '%s' not found\n", oldName);
        return false;
    }

    if (findEntry(newName) >= 0)
    {
        printf("Error: file '%s' already exists\n", newName);
        return false;
    }

    DirEntry entry;
    readDirEntry(oldIdx, &entry);

    char fat16Name[11];
    toFAT16Name(newName, fat16Name);
    memcpy(fat16Name, entry.filename, 8);
    memcpy(fat16Name + 8, entry.extension, 3);

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

void FAT16::listFiles(const char *path)
{
    if (!mounted)
    {
        printf("Error: filesystem not mounted\n");
        return;
    }

    int idx = findEntry(path);
    if (idx < 0)
    {
        printf("Error: directory '%s' not found\n", path);
        return;
    }

    DirEntry dirEntry;
    readDirEntry(idx, &dirEntry);
    if (!(dirEntry.attributes & ATTR_DIRECTORY))
    {
        printf("Error: '%s' is not a directory\n", path);
        return;
    }

    printf("FILENAME       SIZE      CLUSTER\n");
    printf("-------------------------------\n");

    int count = 0;
    DirEntry entry;
    for (int i = 0; i < ENTRIES_PER_SECTOR; ++i)
    {
        readDirEntryInDir(dirEntry.firstCluster, i, &entry);
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
