/** File/File.c
 *
 * (C) Charity Enol
 *
 * 文件管理入口。
 */

#include <File/File.h>

#include <string.h>

static FAT32_VOLUME SYSTEM_VOLUME;

void InitFile(void)
{
    memset(&SYSTEM_VOLUME, 0, sizeof(SYSTEM_VOLUME));
}

bool IsFileSystemReady(void)
{
    return IsFAT32Mounted(&SYSTEM_VOLUME);
}

FAT32_VOLUME *GetSystemVolume(void)
{
    return &SYSTEM_VOLUME;
}

bool MountSystemFAT32(FAT32_READ_SECTORS read_sectors, void *context)
{
    return MountFAT32(&SYSTEM_VOLUME, read_sectors, context);
}
