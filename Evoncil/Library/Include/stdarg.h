#ifndef STDARG_H
#define STDARG_H

typedef __builtin_va_list va_list;

#define va_start(argument_list, last_argument) __builtin_va_start(argument_list, last_argument)
#define va_arg(argument_list, type) __builtin_va_arg(argument_list, type)
#define va_end(argument_list) __builtin_va_end(argument_list)
#define va_copy(destination, source) __builtin_va_copy(destination, source)

#endif // STDARG_H
