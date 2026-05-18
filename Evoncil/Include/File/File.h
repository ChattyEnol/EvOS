/** File/File.h
 *
 * (C) Charity Enol
 *
 * 文件管理入口。
 */

#ifndef FILE_FILE_H
#define FILE_FILE_H

#include <File/FAT32.h>
#include <stdbool.h>

void InitFile(void);
bool IsFileSystemReady(void);
FAT32_VOLUME *GetSystemVolume(void);
bool MountSystemFAT32(FAT32_READ_SECTORS read_sectors, void *context);

#endif // FILE_FILE_H
