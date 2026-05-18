#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef struct
{
    unsigned long long FrameBufferBase; // (EFI_PHYSICAL_ADDRESS) 显存首地址。
    unsigned long long FrameBufferSize; // (UINTN)
    unsigned int HorizontalResolution; // (UINT32)
    unsigned int VerticalResolution; // (UINT32)
    unsigned int PixelsPerScanLine; // (UINT32)
} GRAPHICS;

#endif // GRAPHICS_H