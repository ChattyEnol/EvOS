#include <string.h>

void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *destinationIndex = (unsigned char *)destination;
    const unsigned char *sourceIndex = (const unsigned char *)source;

    for (size_t index = 0; index < size; index++)
        destinationIndex[index] = sourceIndex[index];

    return destination;
}

void *memmove(void *destination, const void *source, size_t size)
{
    unsigned char *destinationIndex = (unsigned char *)destination;
    const unsigned char *sourceIndex = (const unsigned char *)source;

    if (destinationIndex == sourceIndex || size == 0)
        return destination;

    if (destinationIndex < sourceIndex)
    {
        for (size_t index = 0; index < size; index++)
            destinationIndex[index] = sourceIndex[index];
    }
    else
    {
        for (size_t index = size; index > 0; index--)
            destinationIndex[index - 1] = sourceIndex[index - 1];
    }

    return destination;
}

void *memset(void *destination, int value, size_t size)
{
    unsigned char *destinationIndex = (unsigned char *)destination;

    for (size_t index = 0; index < size; index++)
        destinationIndex[index] = (unsigned char)value;

    return destination;
}

int memcmp(const void *left, const void *right, size_t size)
{
    const unsigned char *leftIndex = (const unsigned char *)left;
    const unsigned char *rightIndex = (const unsigned char *)right;

    for (size_t index = 0; index < size; index++)
    {
        if (leftIndex[index] != rightIndex[index])
            return (int)leftIndex[index] - (int)rightIndex[index];
    }

    return 0;
}

size_t strlen(const char *string)
{
    size_t length = 0;

    while (string[length] != '\0')
        length++;

    return length;
}

int strcmp(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right)
    {
        left++;
        right++;
    }

    return (int)(unsigned char)*left - (int)(unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t size)
{
    for (size_t index = 0; index < size; index++)
    {
        unsigned char leftCharacter = (unsigned char)left[index];
        unsigned char rightCharacter = (unsigned char)right[index];

        if (leftCharacter != rightCharacter)
            return (int)leftCharacter - (int)rightCharacter;

        if (leftCharacter == '\0')
            return 0;
    }

    return 0;
}

char *strcpy(char *destination, const char *source)
{
    char *destinationStart = destination;

    while ((*destination++ = *source++) != '\0')
        ;

    return destinationStart;
}

char *strncpy(char *destination, const char *source, size_t size)
{
    size_t index = 0;

    for (; index < size && source[index] != '\0'; index++)
        destination[index] = source[index];

    for (; index < size; index++)
        destination[index] = '\0';

    return destination;
}

char *strchr(const char *string, int character)
{
    char target = (char)character;

    while (*string != '\0')
    {
        if (*string == target)
            return (char *)string;

        string++;
    }

    if (target == '\0')
        return (char *)string;

    return NULL;
}
