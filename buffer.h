#pragma once

#include <stdbool.h>
#include <stddef.h>

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct buffer
{
    char * b;
    size_t len;
    size_t capacity;
};


bool
linenoise_buffer_init(struct buffer * ab, size_t initial_capacity);

/*
 * Append characaters to the buffer.
 * Return true if successful, else false.
 */
bool
linenoise_buffer_append(struct buffer * ab, char const * s, size_t len);

int linenoise_buffer_snprintf(
    struct buffer * ab,
    char * buf, size_t buf_size,
    char const * fmt, ...);

bool
linenoise_buffer_grow(struct buffer * ab, size_t amount);

void
linenoise_buffer_free(struct buffer * ab);

