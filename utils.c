#include <ctype.h>
#include <stdio.h>

static int iswhite(int c)
{
    return c == ' ' || c == '\t';
}

/*
 * skipwhite: skip over ' ' and '\t'.
 */
char *skipwhite(char *q)
{
    char *p = q;

    /* skip to next non-white */
    while (p && iswhite(*(unsigned char *)p))
        ++p;
    return p;
}

/*
 * Return the value of a single hex character.
 * Only valid when the argument is '0' - '9', 'A' - 'F' or 'a' - 'f'.
 */
int hex2nr(int c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return c - '0';
}
