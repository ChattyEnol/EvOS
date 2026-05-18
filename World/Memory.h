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