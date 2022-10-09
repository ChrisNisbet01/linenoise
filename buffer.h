#pragma once

#include <stdbool.h>
#include <stddef.h>

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf
{
    char * b;
    size_t len;
    size_t capacity;
};


void linenoise_abInit(struct abuf * ab, size_t initial_capacity);

/*
 * Append characaters to the buffer.
 * Return true if successful, else false.
 */
bool linenoise_abAppend(struct abuf * ab, char const * s, size_t len);

void linenoise_abFree(struct abuf * ab);

