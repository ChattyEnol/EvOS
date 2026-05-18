/** UI/Font.h
 * 
 * 其实这个是参考的 Linux。
 */

#ifndef DRIVERS_FONT_H
#define DRIVERS_FONT_H

#include <stdint.h>

#define FONT_EXTRA_WORDS 4

typedef struct
{
    uint32_t extra[FONT_EXTRA_WORDS];
    uint8_t data[];
} FONT_DATA;

typedef struct
{
    int idx;
    const char *name;
    uint32_t width;
    uint32_t height;
    uint32_t charcount;
    const uint8_t *data;
    int pref;
} FONT_DESC;

#define VGA8x16_IDX 0
#define EXPORT_SYMBOL(symbol)

extern const FONT_DESC font_vga_8x16;

#endif