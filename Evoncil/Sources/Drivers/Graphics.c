/** Drivers/Graphics.c
 *
 * (C) Charity Enol
 *
 * 显卡驱动实现。
 * 提供像素级的图形操作接口。
 */

#include <Noyau/Memory.h>
#include <Drivers/Graphics.h>
#include <stddef.h>

uintptr_t FRAME_BUFFER_BASE = NULL;
uint64_t FRAME_BUFFER_SIZE = NULL;
uint32_t HORIZONTAL_RESOLUTION = NULL;
uint32_t VERTICAL_RESOLUTION = NULL;
uint32_t PIXELS_PER_SCAN_LINE = NULL;

void InitGraphics(GRAPHICS *graphics)
{
    FRAME_BUFFER_BASE = (uintptr_t)graphics->FrameBufferBase;
    FRAME_BUFFER_SIZE = (uint64_t)graphics->FrameBufferSize;
    HORIZONTAL_RESOLUTION = (uint32_t)graphics->HorizontalResolution;
    VERTICAL_RESOLUTION = (uint32_t)graphics->VerticalResolution;
    PIXELS_PER_SCAN_LINE = (uint32_t)graphics->PixelsPerScanLine;

    void *frameBuffer = MapDeviceMemory(
        FRAME_BUFFER_BASE,
        FRAME_BUFFER_SIZE);
    if (frameBuffer != 0)
        FRAME_BUFFER_BASE = (unsigned long long)frameBuffer;
}

void DrawPixel(uint32_t x, uint32_t y, COLOR color)
{
    if (FRAME_BUFFER_BASE == 0 ||
        x >= HORIZONTAL_RESOLUTION ||
        y >= VERTICAL_RESOLUTION)
        return;

    COLOR *frameBuffer = (COLOR *)FRAME_BUFFER_BASE;
    frameBuffer[y * PIXELS_PER_SCAN_LINE + x] = color;
}

void DrawScreen(COLOR color)
{
    if (FRAME_BUFFER_BASE == 0)
        return;

    COLOR *frameBuffer = (COLOR *)FRAME_BUFFER_BASE;

    for (uint32_t y = 0; y < VERTICAL_RESOLUTION; y++)
        for (uint32_t x = 0; x < HORIZONTAL_RESOLUTION; x++)
            frameBuffer[y * PIXELS_PER_SCAN_LINE + x] = color;
}
