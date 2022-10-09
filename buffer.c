#include "buffer.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_CAPACITY_INCREASE 256

bool
linenoise_buffer_grow(struct buffer * const ab, size_t const amount)
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
linenoise_buffer_init(struct buffer * ab, size_t const initial_capacity)
{
    ab->len = 0;
    ab->capacity = 0;
    ab->b = NULL;

    return linenoise_buffer_grow(ab, initial_capacity);
}

bool
linenoise_buffer_append(
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

        if (!linenoise_buffer_grow(ab, grow_amount))
        {
            return false;
        }
    }

    memcpy(ab->b + ab->len, s, len);
    ab->len = new_len;
    ab->b[ab->len] = '\0';

    return true;
}

void linenoise_buffer_free(struct buffer * ab)
{
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
    ab->capacity = 0;
}

int linenoise_buffer_snprintf(
    struct buffer * const ab,
    char * const buf, size_t const buf_size,
    char const * const fmt, ...)
{
    va_list arg_ptr;

    va_start(arg_ptr, fmt);
    int const res = vsnprintf(buf, buf_size, fmt, arg_ptr);
    va_end(arg_ptr);

    linenoise_buffer_append(ab, buf, strlen(buf));

    return res;
}
