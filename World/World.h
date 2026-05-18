#ifndef WORLD_H
#define WORLD_H

#include "Memory.h"
#include "Graphics.h"

typedef struct
{
    MEMORY Memory;
    GRAPHICS Graphics;
    void *AcpiRoot;
    // // 未来扩展：比如指向内核镜像的起始位置
    // void *KernelBase;
} WORLD;

extern WORLD World;

#endif // WORLD_H
