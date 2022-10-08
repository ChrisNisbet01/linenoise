#pragma once

#include "linenoise_private.h"
#include "buffer.h"

void linenoise_hints_refreshShowHints(
    linenoise_st * const linenoise_ctx,
    struct abuf * ab,
    struct linenoiseState * l,
    int plen);
