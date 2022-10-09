#include "buffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MIN_CAPACITY_INCREASE 256

void linenoise_abInit(struct abuf * ab, size_t const initial_capacity)
{
    ab->len = 0;
    ab->capacity = initial_capacity;
    ab->b = malloc(initial_capacity);
}

void linenoise_abAppend(
    struct abuf * const ab,
    char const * const s,
    size_t const len)
{
    size_t const required_len = ab->len + len;

    /* Grow the buffer if required. */
    if (required_len > ab->capacity)
    {
        size_t extra_bytes = required_len - ab->capacity;

        if (extra_bytes < MIN_CAPACITY_INCREASE)
        {
            extra_bytes = MIN_CAPACITY_INCREASE;
        }
        /* Allow one extra byte for a NUL terminator. */
        size_t const new_capacity = ab->capacity + extra_bytes;
        char * const new_buf = realloc(ab->b, new_capacity + 1);

        if (new_buf == NULL)
        {
            return;
        }
        ab->b = new_buf;
        ab->capacity = new_capacity;
    }

    memcpy(ab->b + ab->len, s, len);
    ab->len = required_len;
}

void linenoise_abFree(struct abuf * ab)
{
    free(ab->b);
}


