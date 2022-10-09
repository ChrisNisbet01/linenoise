#include "linenoise_hints.h"

#if WITH_HINTS
#include <string.h>

/* Helper of refreshSingleLine() and refreshMultiLine() to show hints
 * to the right of the prompt. */
void
linenoise_hints_refreshShowHints(
    linenoise_st const * const linenoise_ctx,
    struct buffer * const ab,
    struct linenoiseState const * const l,
    int const prompt_len)
{
    if (linenoise_ctx->options.hintsCallback && prompt_len + l->len < l->cols)
    {
        int color = -1, bold = 0;
        char * hint = linenoise_ctx->options.hintsCallback(l->line_buf->b, &color, &bold);
        if (hint != NULL)
        {
            char seq[64];
            int const hintmaxlen = l->cols - (prompt_len + l->len);
            int hintlen = strlen(hint);

            if (hintlen > hintmaxlen)
            {
                hintlen = hintmaxlen;
            }
            if (bold == 1 && color == -1)
            {
                color = 37;
            }
            if (color != -1 || bold != 0)
            {
                linenoise_buffer_snprintf(ab, seq, sizeof seq, "\033[%d;%d;49m", bold, color);
            }
            linenoise_buffer_append(ab, hint, hintlen);
            if (color != -1 || bold != 0)
            {
                linenoise_buffer_append(ab, "\033[0m", strlen("\033[0m"));
            }
            /* Call the function to free the hint returned. */
            if (linenoise_ctx->options.freeHintsCallback)
            {
                linenoise_ctx->options.freeHintsCallback(hint);
            }
        }
    }
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void
linenoiseSetHintsCallback(linenoise_st * const linenoise_ctx,
                          linenoiseHintsCallback * const fn)
{
    linenoise_ctx->options.hintsCallback = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoiseSetHintsCallback(). */
void
linenoiseSetFreeHintsCallback(linenoise_st * const linenoise_ctx,
                              linenoiseFreeHintsCallback * const fn)
{
    linenoise_ctx->options.freeHintsCallback = fn;
}

#endif

