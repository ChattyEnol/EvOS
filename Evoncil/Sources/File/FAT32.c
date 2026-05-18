/** File/FAT32.c
 *
 * (C) Charity Enol
 *
 * FAT32 只读实现。
 */

#include <File/FAT32.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FAT32_BOOT_SIGNATURE_OFFSET 510
#define FAT32_BOOT_SIGNATURE 0xAA55
#define FAT32_END_OF_CLUSTER_CHAIN 0x0FFFFFF8
#define FAT32_CLUSTER_MASK 0x0FFFFFFF
#define FAT32_DIRECTORY_ENTRY_SIZE 32
#define FAT32_ATTRIBUTE_LONG_NAME 0x0F
#define FAT32_ATTRIBUTE_DIRECTORY 0x10

typedef struct
{
    uint8_t Jump[3];
    uint8_t OEMName[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectorCount;
    uint8_t FatCount;
    uint16_t RootEntryCount;
    uint16_t TotalSectors16;
    uint8_t Media;
    uint16_t SectorsPerFat16;
    uint16_t SectorsPerTrack;
    uint16_t HeadCount;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    uint32_t SectorsPerFat32;
    uint16_t ExtendedFlags;
    uint16_t FileSystemVersion;
    uint32_t RootCluster;
    uint16_t FileSystemInfo;
    uint16_t BackupBootSector;
    uint8_t Reserved[12];
    uint8_t DriveNumber;
    uint8_t Reserved1;
    uint8_t BootSignature;
    uint32_t VolumeID;
    uint8_t VolumeLabel[11];
    uint8_t FileSystemType[8];
} __attribute__((packed)) FAT32_BOOT_SECTOR;

typedef struct
{
    uint8_t Name[11];
    uint8_t Attribute;
    uint8_t NTReserved;
    uint8_t CreationTimeTenths;
    uint16_t CreationTime;
    uint16_t CreationDate;
    uint16_t LastAccessDate;
    uint16_t FirstClusterHigh;
    uint16_t WriteTime;
    uint16_t WriteDate;
    uint16_t FirstClusterLow;
    uint32_t FileSize;
} __attribute__((packed)) FAT32_DIRECTORY_ENTRY;

static uint8_t SECTOR_BUFFER[512];
static uint8_t CLUSTER_BUFFER[4096];

static uint32_t GetClusterSector(FAT32_VOLUME *volume, uint32_t cluster);
static uint32_t GetNextCluster(FAT32_VOLUME *volume, uint32_t cluster);
static bool ReadCluster(FAT32_VOLUME *volume, uint32_t cluster, void *buffer);
static bool IsEndOfClusterChain(uint32_t cluster);
static void MakeShortName(const FAT32_DIRECTORY_ENTRY *entry, char *name);
static bool MatchShortName(const char *left, const char *right);
static bool IsValidBootSector(const uint8_t *sector);

bool MountFAT32(FAT32_VOLUME *volume, FAT32_READ_SECTORS read_sectors, void *context)
{
    if (volume == NULL || read_sectors == NULL)
        return false;

    memset(volume, 0, sizeof(FAT32_VOLUME));
    memset(SECTOR_BUFFER, 0, sizeof(SECTOR_BUFFER));

    if (!read_sectors(0, 1, SECTOR_BUFFER, context))
        return false;

    if (!IsValidBootSector(SECTOR_BUFFER))
        return false;

    FAT32_BOOT_SECTOR *bootSector = (FAT32_BOOT_SECTOR *)SECTOR_BUFFER;
    if (bootSector->BytesPerSector != 512 ||
        bootSector->SectorsPerCluster == 0 ||
        bootSector->SectorsPerCluster > 8 ||
        bootSector->FatCount == 0 ||
        bootSector->SectorsPerFat32 == 0 ||
        bootSector->RootCluster < 2)
        return false;

    volume->ReadSectors = read_sectors;
    volume->Context = context;
    volume->BytesPerSector = bootSector->BytesPerSector;
    volume->SectorsPerCluster = bootSector->SectorsPerCluster;
    volume->ReservedSectorCount = bootSector->ReservedSectorCount;
    volume->FatCount = bootSector->FatCount;
    volume->SectorsPerFat = bootSector->SectorsPerFat32;
    volume->RootCluster = bootSector->RootCluster;
    volume->FatStartSector = volume->ReservedSectorCount;
    volume->DataStartSector =
        volume->ReservedSectorCount + volume->FatCount * volume->SectorsPerFat;
    volume->Mounted = true;
    return true;
}

bool FindFAT32RootFile(FAT32_VOLUME *volume, const char *name, FAT32_FILE *file)
{
    if (!IsFAT32Mounted(volume) || name == NULL || file == NULL)
        return false;

    uint32_t cluster = volume->RootCluster;
    uint32_t clusterSize = volume->BytesPerSector * volume->SectorsPerCluster;

    while (!IsEndOfClusterChain(cluster))
    {
        if (!ReadCluster(volume, cluster, CLUSTER_BUFFER))
            return false;

        for (uint32_t offset = 0; offset < clusterSize; offset += FAT32_DIRECTORY_ENTRY_SIZE)
        {
            FAT32_DIRECTORY_ENTRY *entry = (FAT32_DIRECTORY_ENTRY *)(CLUSTER_BUFFER + offset);

            if (entry->Name[0] == 0x00)
                return false;

            if (entry->Name[0] == 0xE5 ||
                entry->Attribute == FAT32_ATTRIBUTE_LONG_NAME)
                continue;

            char shortName[FAT32_SHORT_NAME_SIZE];
            MakeShortName(entry, shortName);
            if (!MatchShortName(shortName, name))
                continue;

            memset(file, 0, sizeof(FAT32_FILE));
            strcpy(file->Name, shortName);
            file->FirstCluster =
                ((uint32_t)entry->FirstClusterHigh << 16) | entry->FirstClusterLow;
            file->Size = entry->FileSize;
            file->Attribute = entry->Attribute;
            file->Directory = (entry->Attribute & FAT32_ATTRIBUTE_DIRECTORY) != 0;
            return true;
        }

        cluster = GetNextCluster(volume, cluster);
    }

    return false;
}

bool ReadFAT32File(FAT32_VOLUME *volume, const FAT32_FILE *file, void *buffer, uint32_t size)
{
    if (!IsFAT32Mounted(volume) || file == NULL || buffer == NULL || file->Directory)
        return false;

    uint32_t bytesToRead = size < file->Size ? size : file->Size;
    uint32_t bytesRead = 0;
    uint32_t cluster = file->FirstCluster;
    uint32_t clusterSize = volume->BytesPerSector * volume->SectorsPerCluster;
    uint8_t *output = (uint8_t *)buffer;

    while (bytesRead < bytesToRead && !IsEndOfClusterChain(cluster))
    {
        if (!ReadCluster(volume, cluster, CLUSTER_BUFFER))
            return false;

        uint32_t bytesLeft = bytesToRead - bytesRead;
        uint32_t copySize = bytesLeft < clusterSize ? bytesLeft : clusterSize;
        memcpy(output + bytesRead, CLUSTER_BUFFER, copySize);

        bytesRead += copySize;
        cluster = GetNextCluster(volume, cluster);
    }

    return bytesRead == bytesToRead;
}

bool IsFAT32Mounted(FAT32_VOLUME *volume)
{
    return volume != NULL && volume->Mounted;
}

static uint32_t GetClusterSector(FAT32_VOLUME *volume, uint32_t cluster)
{
    return volume->DataStartSector + (cluster - 2) * volume->SectorsPerCluster;
}

static uint32_t GetNextCluster(FAT32_VOLUME *volume, uint32_t cluster)
{
    uint32_t fatOffset = cluster * sizeof(uint32_t);
    uint32_t sector = volume->FatStartSector + fatOffset / volume->BytesPerSector;
    uint32_t offset = fatOffset % volume->BytesPerSector;

    if (!volume->ReadSectors(sector, 1, SECTOR_BUFFER, volume->Context))
        return FAT32_END_OF_CLUSTER_CHAIN;

    return (*(uint32_t *)(SECTOR_BUFFER + offset)) & FAT32_CLUSTER_MASK;
}

static bool ReadCluster(FAT32_VOLUME *volume, uint32_t cluster, void *buffer)
{
    return volume->ReadSectors(
        GetClusterSector(volume, cluster),
        volume->SectorsPerCluster,
        buffer,
        volume->Context);
}

static bool IsEndOfClusterChain(uint32_t cluster)
{
    return cluster >= FAT32_END_OF_CLUSTER_CHAIN || cluster < 2;
}

static void MakeShortName(const FAT32_DIRECTORY_ENTRY *entry, char *name)
{
    uint32_t outputIndex = 0;
    uint32_t baseEnd = 8;

    while (baseEnd > 0 && entry->Name[baseEnd - 1] == ' ')
        baseEnd--;

    for (uint32_t index = 0; index < baseEnd; index++)
        name[outputIndex++] = (char)entry->Name[index];

    uint32_t extensionEnd = 11;
    while (extensionEnd > 8 && entry->Name[extensionEnd - 1] == ' ')
        extensionEnd--;

    if (extensionEnd > 8)
    {
        name[outputIndex++] = '.';
        for (uint32_t index = 8; index < extensionEnd; index++)
            name[outputIndex++] = (char)entry->Name[index];
    }

    name[outputIndex] = '\0';
}

static bool MatchShortName(const char *left, const char *right)
{
    uint32_t index = 0;

    while (left[index] != '\0' && right[index] != '\0')
    {
        char leftCharacter = left[index];
        char rightCharacter = right[index];

        if (leftCharacter >= 'a' && leftCharacter <= 'z')
            leftCharacter = (char)(leftCharacter - 'a' + 'A');
        if (rightCharacter >= 'a' && rightCharacter <= 'z')
            rightCharacter = (char)(rightCharacter - 'a' + 'A');

        if (leftCharacter != rightCharacter)
            return false;

        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

static bool IsValidBootSector(const uint8_t *sector)
{
    uint16_t signature = *(const uint16_t *)(sector + FAT32_BOOT_SIGNATURE_OFFSET);
    return signature == FAT32_BOOT_SIGNATURE;
}
