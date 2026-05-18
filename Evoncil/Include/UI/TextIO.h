/** UI/TextIO.h
 *
 * (C) Charity Enol
 *
 * 文本输入输出。
 * 字符怎么变成像素、格式化怎么打印，都先放这里。
 */

#ifndef UI_TEXT_IO_H
#define UI_TEXT_IO_H

#include <Drivers/Keyboard.h>
#include <Drivers/Graphics.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    KEY_CODE KeyCode;
    char Character;
} TEXT_INPUT;

void InitTextIO(void);

void DrawChar(int x, int y, char character, COLOR foreground, COLOR background);
void DrawString(int x, int y, const char *string, COLOR foreground, COLOR background);

bool ReadTextInput(TEXT_INPUT *input);

void TextPutChar(char character);
void TextBackspace(void);
void SetTextColor(COLOR foreground, COLOR background);
void SetTextCursor(uint32_t column, uint32_t row);

void kprintf(const char *fmt, ...);

#endif // UI_TEXT_IO_H
