/** World/Memory.h
 * 
 * (C) Charity Enol
 * 
 * 这里是 `MEMORY` 结构体，世界树的内存枝桠。
 * 包含了 UEFI 的内存遗产。
 */

#ifndef MEMORY_H
#define MEMORY_H

typedef struct
{
    unsigned long long MemoryMapSize; // (UINTN) 整个图的大小 (bytes)
    void *MemoryMap;                  // (EFI_MEMORY_DESCRIPTOR *) 内存图的首地址
    unsigned long long MapKey;
    unsigned long long DescriptorSize; // 每个描述符的大小
    unsigned int DescriptorVersion;    // (UINT32)
} MEMORY;

#endif // MEMORY_H