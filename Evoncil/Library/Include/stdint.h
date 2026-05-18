#ifndef STDINT_H
#define STDINT_H

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed long long intptr_t;
typedef unsigned long long uintptr_t;

#define UINT8_MAX 0xFFu
#define UINT16_MAX 0xFFFFu
#define UINT32_MAX 0xFFFFFFFFu
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFull

#endif // STDINT_H
