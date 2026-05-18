#include <Uefi.h>
#include <EvLoader.h>
#include <stdio.h>
#include <stdarg.h>

#define BUFFER_SIZE 512

static inline int U64ToString(UINT64 value, int base, CHAR16 *str);
static inline void OutBuffer(CHAR16 *buffer_in, CHAR16 *buffer_out, UINTN *buffer_out_index, int min_length);

int printf(const char *format, ...)
{
    CHAR16 bufferOut[BUFFER_SIZE];
    UINTN bufferOutIndex = 0;
    va_list args;
    va_start(args, format);

    for (int formatIndex = 0; format[formatIndex] != '\0' && bufferOutIndex < BUFFER_SIZE - 2; formatIndex++)
    {
        if (format[formatIndex] == '%' && format[formatIndex + 1] != '\0')
        {
            formatIndex++;
            if (format[formatIndex] == 's') // String
            {
                char *s = va_arg(args, char *);
                while (*s && bufferOutIndex < BUFFER_SIZE - 2)
                    bufferOut[bufferOutIndex++] = (CHAR16)(*s++);
            }
            else if (format[formatIndex] == 'p') // 指针：补全 16 位。
            {
                UINT64 p = va_arg(args, UINT64);
                CHAR16 hexBuffer[65];
                U64ToString(p, 16, hexBuffer);
                OutBuffer(hexBuffer, bufferOut, &bufferOutIndex, 16);
            }
            else if (format[formatIndex] == 'x') // 普通 16 进制：按需显示
            {
                UINT64 x = va_arg(args, UINT64);
                CHAR16 hexBuffer[65];
                U64ToString(x, 16, hexBuffer);
                OutBuffer(hexBuffer, bufferOut, &bufferOutIndex, 0);
            }
            else if (format[formatIndex] == 'd') // 十进制。
            {
                UINT32 n = va_arg(args, UINT32);
                CHAR16 decBuffer[65];
                U64ToString(n, 10, decBuffer);
                OutBuffer(decBuffer, bufferOut, &bufferOutIndex, 0);
            }
            // 加新的？那就 else-if 吧。
        }
        else
            bufferOut[bufferOutIndex++] = (CHAR16)format[formatIndex];
    }

    bufferOut[bufferOutIndex] = L'\0';
    EV_LOAD(SYSTEM_TABLE->ConOut->OutputString(SYSTEM_TABLE->ConOut, bufferOut));
    va_end(args);
    return (int)bufferOutIndex;
}

static inline int U64ToString(UINT64 value_in, int base, CHAR16 *string_out)
{
    const char *HEX_DIGITS = "0123456789ABCDEF";
    int charCount = 0;

    // 这里是倒序填入 string_out 的。
    if (value_in == 0)
        string_out[charCount++] = L'0';
    else
    {
        while (value_in > 0)
        {
            string_out[charCount++] = (CHAR16)HEX_DIGITS[value_in % base];
            value_in /= base;
        }
    }
    string_out[charCount] = L'\0';
    // 填好了要翻转。
    for (int i = 0; i < charCount / 2; i++)
    {
        CHAR16 tempChar = string_out[i];
        string_out[i] = string_out[charCount - 1 - i];
        string_out[charCount - 1 - i] = tempChar;
    }
    return charCount;
}

static inline void OutBuffer(CHAR16 *buffer_in, CHAR16 *buffer_out, UINTN *buffer_out_index, int min_length)
{
    int len = 0;
    while (buffer_in[len] != L'\0')
        len++;
    // 高位先补 0。
    while (len < min_length && *buffer_out_index < BUFFER_SIZE - 2)
    {
        buffer_out[(*buffer_out_index)++] = L'0';
        min_length--;
    }
    for (int k = 0; buffer_in[k] != L'\0' && *buffer_out_index < BUFFER_SIZE - 2; k++)
        buffer_out[(*buffer_out_index)++] = buffer_in[k];
}
