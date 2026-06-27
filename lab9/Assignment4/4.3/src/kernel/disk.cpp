#include "disk.h"
#include "asm_utils.h"
#include "stdio.h"

DiskDriver::DiskDriver()
{
    commandCount = 0;
    wordIOCount = 0;
}

void DiskDriver::initialize()
{
    printf("ATA disk driver initialized.\n");
}

void DiskDriver::waitReady()
{
    uint8 status;
    do
    {
        asm_in_port(ATA_STATUS, &status);
    } while (status & ATA_STATUS_BSY);
}

void DiskDriver::selectSector(uint32 lba, uint8 count)
{
    waitReady();
    asm_out_port(ATA_SECT_CNT, count);
    asm_out_port(ATA_LBA_LOW, (uint8)(lba & 0xFF));
    asm_out_port(ATA_LBA_MID, (uint8)((lba >> 8) & 0xFF));
    asm_out_port(ATA_LBA_HIGH, (uint8)((lba >> 16) & 0xFF));
    // LBA 模式, 主盘, 高4位LBA
    asm_out_port(ATA_DRIVE_HEAD, (uint8)(0xE0 | ((lba >> 24) & 0x0F)));
}

bool DiskDriver::readSectors(uint32 lba, uint32 count, void *buffer)
{
    uint16 *buf = (uint16 *)buffer;

    for (uint32 s = 0; s < count; ++s)
    {
        selectSector(lba + s, 1);
        asm_out_port(ATA_COMMAND, ATA_CMD_READ);
        commandCount++;

        // 等待数据就绪
        uint8 status;
        do
        {
            asm_in_port(ATA_STATUS, &status);
        } while (!(status & ATA_STATUS_DRQ));

        // 每个扇区 256 个 16-bit 字
        for (int i = 0; i < 256; ++i)
        {
            asm_in_port_word(ATA_DATA, &buf[s * 256 + i]);
            wordIOCount++;
        }
    }

    return true;
}

bool DiskDriver::writeSectors(uint32 lba, uint32 count, const void *buffer)
{
    const uint16 *buf = (const uint16 *)buffer;

    for (uint32 s = 0; s < count; ++s)
    {
        selectSector(lba + s, 1);
        asm_out_port(ATA_COMMAND, ATA_CMD_WRITE);
        commandCount++;

        // 等待数据就绪
        uint8 status;
        do
        {
            asm_in_port(ATA_STATUS, &status);
        } while (!(status & ATA_STATUS_DRQ));

        // 每个扇区 256 个 16-bit 字
        for (int i = 0; i < 256; ++i)
        {
            asm_out_port_word(ATA_DATA, buf[s * 256 + i]);
            wordIOCount++;
        }
    }

    return true;
}

bool DiskDriver::readSectorsOptimized(uint32 lba, uint32 count, void *buffer)
{
    uint32 before = commandCount;
    bool ok = readSectors(lba, count, buffer);
    if (ok && count > 0)
        commandCount = before + 1;
    return ok;
}

bool DiskDriver::writeSectorsOptimized(uint32 lba, uint32 count, const void *buffer)
{
    uint32 before = commandCount;
    bool ok = writeSectors(lba, count, buffer);
    if (ok && count > 0)
        commandCount = before + 1;
    return ok;
}

void DiskDriver::resetCounters()
{
    commandCount = 0;
    wordIOCount = 0;
}

uint32 DiskDriver::getCommandCount()
{
    return commandCount;
}

uint32 DiskDriver::getWordIOCount()
{
    return wordIOCount;
}
