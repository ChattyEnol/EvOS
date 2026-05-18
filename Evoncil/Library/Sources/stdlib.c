#include <ctype.h>
#include <stdlib.h>

int abs(int value)
{
    return value < 0 ? -value : value;
}

long labs(long value)
{
    return value < 0 ? -value : value;
}

static long ConvertStringToLong(const char *string)
{
    long sign = 1;
    long value = 0;

    while (isspace(*string))
        string++;

    if (*string == '-')
    {
        sign = -1;
        string++;
    }
    else if (*string == '+')
    {
        string++;
    }

    while (isdigit(*string))
    {
        value = value * 10 + (*string - '0');
        string++;
    }

    return value * sign;
}

int atoi(const char *string)
{
    return (int)ConvertStringToLong(string);
}

long atol(const char *string)
{
    return ConvertStringToLong(string);
}
