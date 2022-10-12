#pragma once

#include "linenoise_private.h"
#include "buffer.h"

void linenoise_hints_refreshShowHints(
    linenoise_st const * linenoise_ctx,
    struct buffer * ab,
    struct linenoise_state const * l,
    int plen);

