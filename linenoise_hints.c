#include "linenoise_hints.h"

#if WITH_HINTS
#include <string.h>

/* Helper of refreshSingleLine() and refreshMultiLine() to show hints
 * to the right of the prompt. */
void linenoise_hints_refreshShowHints(
    linenoise_st * const linenoise_ctx,
    struct abuf * ab,
    struct linenoiseState * l, int plen)
{
    char seq[64];
    if (linenoise_ctx->options.hintsCallback && plen + l->len < l->cols)
    {
        int color = -1, bold = 0;
        char * hint = linenoise_ctx->options.hintsCallback(l->buf, &color, &bold);
        if (hint)
        {
            int hintlen = strlen(hint);
            int hintmaxlen = l->cols - (plen + l->len);
            if (hintlen > hintmaxlen)
                hintlen = hintmaxlen;
            if (bold == 1 && color == -1)
                color = 37;
            if (color != -1 || bold != 0)
            {
                snprintf(seq, sizeof seq, "\033[%d;%d;49m", bold, color);
            }
            else
            {
                seq[0] = '\0';
            }
            linenoise_abAppend(ab, seq, strlen(seq));
            linenoise_abAppend(ab, hint, hintlen);
            if (color != -1 || bold != 0)
            {
                linenoise_abAppend(ab, "\033[0m", 4);
            }
            /* Call the function to free the hint returned. */
            if (linenoise_ctx->options.freeHintsCallback)
                linenoise_ctx->options.freeHintsCallback(hint);
        }
    }
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void linenoiseSetHintsCallback(linenoise_st * const linenoise_ctx,
                               linenoiseHintsCallback * const fn)
{
    linenoise_ctx->options.hintsCallback = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoiseSetHintsCallback(). */
void linenoiseSetFreeHintsCallback(linenoise_st * const linenoise_ctx,
                                   linenoiseFreeHintsCallback * const fn)
{
    linenoise_ctx->options.freeHintsCallback = fn;
}

#endif

