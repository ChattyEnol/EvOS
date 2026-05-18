#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int memcmp(const void *left, const void *right, size_t size);

size_t strlen(const char *string);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t size);
char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t size);
char *strchr(const char *string, int character);

#endif // STRING_H
