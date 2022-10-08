#include "buffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* =========================== Line editing ================================= */

void linenoise_abInit(struct abuf * ab)
{
    ab->b = NULL;
    ab->len = 0;
}

void linenoise_abAppend(struct abuf * ab, const char * s, int len)
{
    char * new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

void linenoise_abFree(struct abuf * ab)
{
    free(ab->b);
}


