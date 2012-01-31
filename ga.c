#include "ga.h"

#include <stdlib.h>
#include <string.h>

/************************************************************************
 * Functions for handling growing arrays.
 */

/*
 * Clear an allocated growing array.
 */
void ga_clear(garray_T *gap)
{
    free(gap->ga_data);
    ga_init(gap);
}

/*
 * Clear a growing array that contains a list of strings.
 */
void ga_clear_strings(garray_T *gap)
{
    size_t i;
    for (i = 0; i < gap->ga_len; ++i)
        free(((char **)(gap->ga_data))[i]);
    ga_clear(gap);
}

/*
 * Initialize a growing array.        Don't forget to set ga_itemsize and
 * ga_growsize!  Or use ga_init2().
 */
void ga_init(garray_T *gap)
{
    gap->ga_data = 0;
    gap->ga_maxlen = 0;
    gap->ga_len = 0;
}

void ga_init2(garray_T *gap, size_t itemsize, size_t growsize)
{
    ga_init(gap);
    gap->ga_itemsize = itemsize;
    gap->ga_growsize = growsize;
}

/*
 * Make room in growing array "gap" for at least "n" items.
 * Return -1 for failure, 0 otherwise.
 */
int ga_grow(garray_T *gap, size_t n)
{
    size_t        len;
    char        *pp;

    if (gap->ga_maxlen - gap->ga_len < n) {
        if (n < gap->ga_growsize)
            n = gap->ga_growsize;
        len = gap->ga_itemsize * (gap->ga_len + n);
        pp = calloc(len, sizeof(char));
        if (!pp)
            return -1;
        gap->ga_maxlen = gap->ga_len + n;
        if (gap->ga_data) {
            memmove(pp, gap->ga_data, (size_t)(gap->ga_itemsize * gap->ga_len));
            free(gap->ga_data);
        }
        gap->ga_data = pp;
    }
    return 0;
}

/*
 * For a growing array that contains a list of strings: concatenate all the
 * strings with a separating comma.
 * Returns NULL when out of memory.
 */
char *ga_concat_strings(garray_T *gap)
{
    size_t i;
    size_t len = 0;
    char *s;

    for (i = 0; i < gap->ga_len; ++i)
        len += strlen(((char **)(gap->ga_data))[i]) + 1;

    s = malloc(len + 1);
    if (s) {
        *s = '\0';
        for (i = 0; i < gap->ga_len; ++i) {
            if (*s)
                strcat(s, ",");
            strcat(s, ((char **)(gap->ga_data))[i]);
        }
    }
    return s;
}

/*
 * Concatenate a string to a growarray which contains characters.
 * Note: Does NOT copy the NUL at the end!
 */
void ga_concat(garray_T *gap, char *s)
{
    size_t len = strlen(s);

    if (ga_grow(gap, len) == 0) {
        memmove((char *)gap->ga_data + gap->ga_len, s, (size_t)len);
        gap->ga_len += len;
    }
}

/*
 * Append one byte to a growarray which contains bytes.
 */
void ga_append(garray_T *gap, int c)
{
    if (ga_grow(gap, 1) == 0) {
        *((char *)gap->ga_data + gap->ga_len) = c;
        ++gap->ga_len;
    }
}
