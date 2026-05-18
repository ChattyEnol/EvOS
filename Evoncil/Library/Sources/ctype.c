#include <ctype.h>

int isalpha(int character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z');
}

int isdigit(int character)
{
    return character >= '0' && character <= '9';
}

int isalnum(int character)
{
    return isalpha(character) || isdigit(character);
}

int isspace(int character)
{
    return character == ' ' ||
           character == '\f' ||
           character == '\n' ||
           character == '\r' ||
           character == '\t' ||
           character == '\v';
}

int isupper(int character)
{
    return character >= 'A' && character <= 'Z';
}

int islower(int character)
{
    return character >= 'a' && character <= 'z';
}

int toupper(int character)
{
    if (islower(character))
        return character - ('a' - 'A');

    return character;
}

int tolower(int character)
{
    if (isupper(character))
        return character + ('a' - 'A');

    return character;
}
