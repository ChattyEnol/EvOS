/** File/FAT32.h
 *
 * (C) Charity Enol
 *
 * FAT32 文件系统。
 * 底层块设备先用回调抽象出来，之后可以接 ESP 所在磁盘驱动。
 */

#ifndef FILE_FAT32_H
#define FILE_FAT32_H

#include <stdbool.h>
#include <stdint.h>

#define FAT32_SHORT_NAME_SIZE 12

typedef bool (*FAT32_READ_SECTORS)(
    uint64_t sector,
    uint32_t count,
    void *buffer,
    void *context);

typedef struct
{
    FAT32_READ_SECTORS ReadSectors;
    void *Context;
    uint32_t BytesPerSector;
    uint32_t SectorsPerCluster;
    uint32_t ReservedSectorCount;
    uint32_t FatCount;
    uint32_t SectorsPerFat;
    uint32_t RootCluster;
    uint32_t FatStartSector;
    uint32_t DataStartSector;
    bool Mounted;
} FAT32_VOLUME;

typedef struct
{
    char Name[FAT32_SHORT_NAME_SIZE];
    uint32_t FirstCluster;
    uint32_t Size;
    uint8_t Attribute;
    bool Directory;
} FAT32_FILE;

bool MountFAT32(FAT32_VOLUME *volume, FAT32_READ_SECTORS read_sectors, void *context);
bool FindFAT32RootFile(FAT32_VOLUME *volume, const char *name, FAT32_FILE *file);
bool ReadFAT32File(FAT32_VOLUME *volume, const FAT32_FILE *file, void *buffer, uint32_t size);
bool IsFAT32Mounted(FAT32_VOLUME *volume);

#endif // FILE_FAT32_H
