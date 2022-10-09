#include "buffer.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_CAPACITY_INCREASE 256

bool
linenoise_abGrow(struct buffer * const ab, size_t const amount)
{
    size_t extra_bytes = amount;

    if (extra_bytes < MIN_CAPACITY_INCREASE)
    {
        extra_bytes = MIN_CAPACITY_INCREASE;
    }
    size_t const new_capacity = ab->capacity + extra_bytes;
    /* Allow one extra byte for a NUL terminator. */
    char * const new_buf = realloc(ab->b, new_capacity + 1);

    if (new_buf == NULL)
    {
        return false;
    }
    ab->b = new_buf;
    ab->capacity = new_capacity;

    return true;
}

bool
linenoise_abInit(struct buffer * ab, size_t const initial_capacity)
{
    ab->len = 0;
    ab->capacity = 0;
    ab->b = NULL;

    return linenoise_abGrow(ab, initial_capacity);
}

bool
linenoise_abAppend(
    struct buffer * const ab,
    char const * const s,
    size_t const len)
{
    size_t const new_len = ab->len + len;

    /*
     * Grow the buffer if required.
     * the buffer pointer may be NULL if the buffer wasn't initialised
     * beforehand.
     */
    if (ab->b == NULL || new_len > ab->capacity)
    {
        size_t const grow_amount = new_len - ab->capacity;

        if (!linenoise_abGrow(ab, grow_amount))
        {
            return false;
        }
    }

    memcpy(ab->b + ab->len, s, len);
    ab->len = new_len;
    ab->b[ab->len] = '\0';

    return true;
}

void linenoise_abFree(struct buffer * ab)
{
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
    ab->capacity = 0;
}


