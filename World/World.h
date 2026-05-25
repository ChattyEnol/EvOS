/** World/World.h
 * 
 * (C) Charity Enol
 * 
 * 这里是 `WORLD` 结构体，是我们给 Evoncil 的全世界。
 * 顾名思义，也就是一颗世界树，包含了 OS 需要知道的所有信息。
 * 后期可能会添加更多的内容。
 */

#ifndef WORLD_H
#define WORLD_H

#include "Memory.h"
#include "Graphics.h"

typedef struct
{
    MEMORY Memory;
    GRAPHICS Graphics;
    void *AcpiRoot;
    // 未来扩展：比如指向内核镜像的起始位置。
    // void *KernelBase;
} WORLD;

extern WORLD World;

#endif // WORLD_H
