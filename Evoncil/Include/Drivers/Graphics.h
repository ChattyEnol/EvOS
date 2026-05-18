/** Drivers/Graphics.h
 * 
 * (C) Charity Enol
 * 
 * 显卡驱动头文件。
 * 提供像素级的图形操作接口。
 */

#ifndef DRIVERS_GRAPHICS_H
#define DRIVERS_GRAPHICS_H

#include <World/Graphics.h>
#include <stddef.h>
#include <stdint.h>

// 颜色（ARGB）。
typedef uint32_t COLOR;
#define COLOR_EVONCIL 0xFF6573A1 // 我最爱的颜色！
#define COLOR_PINK 0xFFFFC0CB
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0xFF000000

// 帧缓冲区和分辨率信息，由 InitGraphics 初始化。
extern uintptr_t FRAME_BUFFER_BASE;
extern uint64_t FRAME_BUFFER_SIZE;
extern uint32_t HORIZONTAL_RESOLUTION;
extern uint32_t VERTICAL_RESOLUTION;
extern uint32_t PIXELS_PER_SCAN_LINE;

// 初始化：拿 `GRAPHICS` 结构体指针。
void InitGraphics(GRAPHICS *graphics);

// 画一个点。
void DrawPixel(uint32_t x, uint32_t y, COLOR color);
// 画全世界。
void DrawScreen(COLOR color);

#endif
