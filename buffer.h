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
linenoise_abInit(struct buffer * ab, size_t initial_capacity);

/*
 * Append characaters to the buffer.
 * Return true if successful, else false.
 */
bool
linenoise_abAppend(struct buffer * ab, char const * s, size_t len);

bool
linenoise_abGrow(struct buffer * ab, size_t amount);

void
linenoise_abFree(struct buffer * ab);

