/** World/Graphics.h
 * 
 * (C) Charity Enol
 * 
 * 这里是 `GRAPHICS` 结构体，世界树的一枝枝桠。
 * 包含了显存以及屏幕的相关信息。
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef struct
{
    unsigned long long FrameBufferBase; // (EFI_PHYSICAL_ADDRESS) 显存首地址。
    unsigned long long FrameBufferSize; // (UINTN)
    unsigned int HorizontalResolution;  // (UINT32)
    unsigned int VerticalResolution;    // (UINT32)
    unsigned int PixelsPerScanLine;     // (UINT32)
} GRAPHICS;

#endif // GRAPHICS_H