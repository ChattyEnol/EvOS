/** User/PELoader.h
 *
 * (C) Charity Enol
 *
 * PE32+ 可执行文件加载器接口。
 */

#ifndef USER_PE_LOADER_H
#define USER_PE_LOADER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    const void *FileBase;
    uint64_t FileSize;
    uint64_t PreferredImageBase;
    uint32_t ImageSize;
    uint32_t EntryPointRVA;
    const void *EntryPoint;
} PE_IMAGE;

/**
 * 解析 PE32+ 文件缓冲区，提取镜像入口和基本布局信息。
 */
bool LoadPEImage(const void *file_base, uint64_t file_size, PE_IMAGE *image);

/**
 * 在加载器内部解析一小组 Windows API 名称。
 * 这些 API 最终会转发到 EvOS 的 POSIX 风格用户态封装。
 */
void *ResolveWindowsAPI(const char *name);

#endif // USER_PE_LOADER_H
