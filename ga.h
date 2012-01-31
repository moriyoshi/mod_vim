#ifndef GA_H
#define GA_H

#include <stddef.h>

/*
 * Structure used for growing arrays.
 * This is used to store information that only grows, is deleted all at
 * once, and needs to be accessed by index.  See ga_clear() and ga_grow().
 */
typedef struct growarray
{
    size_t ga_len;		    /* current number of items used */
    size_t ga_maxlen;		    /* maximum number of items possible */
    size_t ga_itemsize;	    /* sizeof(item) */
    size_t ga_growsize;	    /* number of items to grow each time */
    void    *ga_data;		    /* pointer to the first item */
} garray_T;

#define GA_EMPTY    {0, 0, 0, 0, NULL}

void ga_clear(garray_T *gap);
void ga_clear_strings(garray_T *gap);
void ga_init(garray_T *gap);
void ga_init2(garray_T *gap, size_t itemsize, size_t growsize);
int ga_grow(garray_T *gap, size_t n);
char *ga_concat_strings(garray_T *gap);
void ga_concat(garray_T *gap, char *s);
void ga_append(garray_T *gap, int c);

#endif /* GA_H */
