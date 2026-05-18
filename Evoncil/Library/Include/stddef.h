#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned long long size_t;
typedef signed long long ptrdiff_t;

#ifndef NULL
#define NULL 0
#endif

#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif // STDDEF_H
