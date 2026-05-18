/** UI/TextIO.c
 *
 * (C) Charity Enol
 *
 * 文本输出实现。
 * 这层知道字体，也知道光标，但尽量别管“命令该怎么执行”。
 */

#include <UI/TextIO.h>
#include <UI/Font.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

static uint32_t TEXT_CURSOR_COLUMN = 0;
static uint32_t TEXT_CURSOR_ROW = 0;
static COLOR TEXT_FOREGROUND = COLOR_WHITE;
static COLOR TEXT_BACKGROUND = COLOR_BLACK;
static bool TEXT_SHIFT_PRESSED = false;
static bool TEXT_CTRL_PRESSED = false;
static bool TEXT_ALT_PRESSED = false;
static bool TEXT_CAPS_LOCK_ENABLED = false;

static uint32_t GetTextColumnCount(void);
static uint32_t GetTextRowCount(void);
static char GetTextCharacter(KEY_CODE keyCode);
static char GetLetterCharacter(KEY_CODE keyCode);
static char GetNumberCharacter(KEY_CODE keyCode);
static char GetSymbolCharacter(KEY_CODE keyCode);
static void NewLine(void);
static void ScrollText(void);
static void FillTextRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, COLOR color);
static void PrintUnsignedNumber(uint64_t value, uint32_t base, bool uppercase);
static void PrintSignedNumber(int64_t value);

void InitTextIO(void)
{
    TEXT_CURSOR_COLUMN = 0;
    TEXT_CURSOR_ROW = 0;
    TEXT_FOREGROUND = COLOR_WHITE;
    TEXT_BACKGROUND = COLOR_BLACK;
    TEXT_SHIFT_PRESSED = false;
    TEXT_CTRL_PRESSED = false;
    TEXT_ALT_PRESSED = false;
    TEXT_CAPS_LOCK_ENABLED = false;
}

void DrawChar(int x, int y, char character, COLOR foreground, COLOR background)
{
    if (FRAME_BUFFER_BASE == 0)
        return;

    if (x >= (int)HORIZONTAL_RESOLUTION || y >= (int)VERTICAL_RESOLUTION)
        return;

    unsigned char characterIndex = (unsigned char)character;
    const uint8_t *glyph =
        font_vga_8x16.data + characterIndex * font_vga_8x16.height;

    for (uint32_t row = 0; row < font_vga_8x16.height; row++)
    {
        uint8_t line = glyph[row];
        for (uint32_t column = 0; column < font_vga_8x16.width; column++)
        {
            COLOR color = (line & (0x80 >> column)) ? foreground : background;
            DrawPixel((uint32_t)x + column, (uint32_t)y + row, color);
        }
    }
}

void DrawString(int x, int y, const char *string, COLOR foreground, COLOR background)
{
    int currentX = x;

    while (*string != '\0')
    {
        if (*string == '\n')
        {
            currentX = x;
            y += (int)font_vga_8x16.height;
            string++;
            continue;
        }

        DrawChar(currentX, y, *string, foreground, background);
        currentX += (int)font_vga_8x16.width;
        string++;
    }
}

bool ReadTextInput(TEXT_INPUT *input)
{
    KEYBOARD_EVENT event;

    if (input == NULL)
        return false;

    while (ReadKeyboard(&event))
    {
        switch (event.KeyCode)
        {
        case KEY_SHIFT:
            TEXT_SHIFT_PRESSED = event.Pressed;
            continue;
        case KEY_CTRL:
            TEXT_CTRL_PRESSED = event.Pressed;
            continue;
        case KEY_ALT:
            TEXT_ALT_PRESSED = event.Pressed;
            continue;
        case KEY_CAPS_LOCK:
            if (event.Pressed)
                TEXT_CAPS_LOCK_ENABLED = !TEXT_CAPS_LOCK_ENABLED;
            continue;
        default:
            break;
        }

        if (!event.Pressed)
            continue;

        input->KeyCode = event.KeyCode;
        input->Character = GetTextCharacter(event.KeyCode);
        return true;
    }

    return false;
}

void TextPutChar(char character)
{
    if (character == '\r')
        return;

    if (character == '\n')
    {
        NewLine();
        return;
    }

    if (character == '\t')
    {
        for (uint32_t index = 0; index < 4; index++)
            TextPutChar(' ');
        return;
    }

    DrawChar(
        (int)(TEXT_CURSOR_COLUMN * font_vga_8x16.width),
        (int)(TEXT_CURSOR_ROW * font_vga_8x16.height),
        character,
        TEXT_FOREGROUND,
        TEXT_BACKGROUND);

    TEXT_CURSOR_COLUMN++;
    if (TEXT_CURSOR_COLUMN >= GetTextColumnCount())
        NewLine();
}

void TextBackspace(void)
{
    if (TEXT_CURSOR_COLUMN == 0)
    {
        if (TEXT_CURSOR_ROW == 0)
            return;

        TEXT_CURSOR_ROW--;
        TEXT_CURSOR_COLUMN = GetTextColumnCount() - 1;
    }
    else
    {
        TEXT_CURSOR_COLUMN--;
    }

    DrawChar(
        (int)(TEXT_CURSOR_COLUMN * font_vga_8x16.width),
        (int)(TEXT_CURSOR_ROW * font_vga_8x16.height),
        ' ',
        TEXT_FOREGROUND,
        TEXT_BACKGROUND);
}

void SetTextColor(COLOR foreground, COLOR background)
{
    TEXT_FOREGROUND = foreground;
    TEXT_BACKGROUND = background;
}

void SetTextCursor(uint32_t column, uint32_t row)
{
    if (column >= GetTextColumnCount())
        column = GetTextColumnCount() - 1;

    if (row >= GetTextRowCount())
        row = GetTextRowCount() - 1;

    TEXT_CURSOR_COLUMN = column;
    TEXT_CURSOR_ROW = row;
}

void kprintf(const char *fmt, ...)
{
    va_list argumentList;
    va_start(argumentList, fmt);

    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            TextPutChar(*fmt);
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '\0')
            break;

        switch (*fmt)
        {
        case '%':
            TextPutChar('%');
            break;
        case 'c':
            TextPutChar((char)va_arg(argumentList, int));
            break;
        case 's':
        {
            const char *string = va_arg(argumentList, const char *);
            if (string == NULL)
                string = "(null)";

            while (*string != '\0')
            {
                TextPutChar(*string);
                string++;
            }
            break;
        }
        case 'd':
        case 'i':
            PrintSignedNumber((int64_t)va_arg(argumentList, int));
            break;
        case 'u':
            PrintUnsignedNumber((uint64_t)va_arg(argumentList, unsigned int), 10, false);
            break;
        case 'x':
            PrintUnsignedNumber((uint64_t)va_arg(argumentList, unsigned int), 16, false);
            break;
        case 'X':
            PrintUnsignedNumber((uint64_t)va_arg(argumentList, unsigned int), 16, true);
            break;
        case 'p':
            TextPutChar('0');
            TextPutChar('x');
            PrintUnsignedNumber((uint64_t)(uintptr_t)va_arg(argumentList, void *), 16, false);
            break;
        default:
            TextPutChar('%');
            TextPutChar(*fmt);
            break;
        }

        fmt++;
    }

    va_end(argumentList);
}

static uint32_t GetTextColumnCount(void)
{
    return HORIZONTAL_RESOLUTION / font_vga_8x16.width;
}

static uint32_t GetTextRowCount(void)
{
    return VERTICAL_RESOLUTION / font_vga_8x16.height;
}

static char GetTextCharacter(KEY_CODE keyCode)
{
    char character = GetLetterCharacter(keyCode);
    if (character != '\0')
        return character;

    character = GetNumberCharacter(keyCode);
    if (character != '\0')
        return character;

    character = GetSymbolCharacter(keyCode);
    if (character != '\0')
        return character;

    switch (keyCode)
    {
    case KEY_ENTER:
        return '\n';
    case KEY_BACKSPACE:
        return '\b';
    case KEY_TAB:
        return '\t';
    case KEY_SPACE:
        return ' ';
    default:
        return '\0';
    }
}

static char GetLetterCharacter(KEY_CODE keyCode)
{
    char character = '\0';

    if (keyCode >= KEY_A && keyCode <= KEY_Z)
        character = (char)('a' + keyCode - KEY_A);

    if (character == '\0')
        return '\0';

    if (TEXT_SHIFT_PRESSED != TEXT_CAPS_LOCK_ENABLED)
        character = (char)(character - 'a' + 'A');

    return character;
}

static char GetNumberCharacter(KEY_CODE keyCode)
{
    static const char NormalNumbers[] = "1234567890";
    static const char ShiftNumbers[] = "!@#$%^&*()";

    if (keyCode < KEY_1 || keyCode > KEY_0)
        return '\0';

    uint32_t index = (uint32_t)(keyCode - KEY_1);
    return TEXT_SHIFT_PRESSED ? ShiftNumbers[index] : NormalNumbers[index];
}

static char GetSymbolCharacter(KEY_CODE keyCode)
{
    switch (keyCode)
    {
    case KEY_GRAVE:
        return TEXT_SHIFT_PRESSED ? '~' : '`';
    case KEY_MINUS:
        return TEXT_SHIFT_PRESSED ? '_' : '-';
    case KEY_EQUAL:
        return TEXT_SHIFT_PRESSED ? '+' : '=';
    case KEY_LBRACKET:
        return TEXT_SHIFT_PRESSED ? '{' : '[';
    case KEY_RBRACKET:
        return TEXT_SHIFT_PRESSED ? '}' : ']';
    case KEY_BACKSLASH:
        return TEXT_SHIFT_PRESSED ? '|' : '\\';
    case KEY_SEMICOLON:
        return TEXT_SHIFT_PRESSED ? ':' : ';';
    case KEY_QUOTE:
        return TEXT_SHIFT_PRESSED ? '"' : '\'';
    case KEY_COMMA:
        return TEXT_SHIFT_PRESSED ? '<' : ',';
    case KEY_PERIOD:
        return TEXT_SHIFT_PRESSED ? '>' : '.';
    case KEY_SLASH:
        return TEXT_SHIFT_PRESSED ? '?' : '/';
    default:
        return '\0';
    }
}

static void NewLine(void)
{
    TEXT_CURSOR_COLUMN = 0;
    TEXT_CURSOR_ROW++;

    if (TEXT_CURSOR_ROW >= GetTextRowCount())
        ScrollText();
}

static void ScrollText(void)
{
    COLOR *frameBuffer = (COLOR *)FRAME_BUFFER_BASE;
    uint32_t characterHeight = font_vga_8x16.height;
    uint32_t rowsToMove = VERTICAL_RESOLUTION - characterHeight;

    for (uint32_t y = 0; y < rowsToMove; y++)
        for (uint32_t x = 0; x < HORIZONTAL_RESOLUTION; x++)
            frameBuffer[y * PIXELS_PER_SCAN_LINE + x] =
                frameBuffer[(y + characterHeight) * PIXELS_PER_SCAN_LINE + x];

    FillTextRectangle(0, rowsToMove, HORIZONTAL_RESOLUTION, characterHeight, TEXT_BACKGROUND);

    if (TEXT_CURSOR_ROW > 0)
        TEXT_CURSOR_ROW--;
}

static void FillTextRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, COLOR color)
{
    for (uint32_t row = 0; row < height; row++)
        for (uint32_t column = 0; column < width; column++)
            DrawPixel(x + column, y + row, color);
}

static void PrintUnsignedNumber(uint64_t value, uint32_t base, bool uppercase)
{
    char buffer[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    uint32_t index = 0;

    if (value == 0)
    {
        TextPutChar('0');
        return;
    }

    while (value != 0 && index < sizeof(buffer))
    {
        buffer[index] = digits[value % base];
        value /= base;
        index++;
    }

    while (index > 0)
    {
        index--;
        TextPutChar(buffer[index]);
    }
}

static void PrintSignedNumber(int64_t value)
{
    if (value < 0)
    {
        TextPutChar('-');
        PrintUnsignedNumber((uint64_t)(-value), 10, false);
        return;
    }

    PrintUnsignedNumber((uint64_t)value, 10, false);
}
