#include "linenoise_hints.h"

#if WITH_HINTS
#include <string.h>

/* Helper of refreshSingleLine() and refreshMultiLine() to show hints
 * to the right of the prompt. */
void
linenoise_hints_refreshShowHints(
    linenoise_st const * const linenoise_ctx,
    struct buffer * const ab,
    struct linenoise_state const * const l,
    int const prompt_len)
{
    if (linenoise_ctx->options.hints_callback && prompt_len + l->len < l->cols)
    {
        int color = -1, bold = 0;
        char * hint = linenoise_ctx->options.hints_callback(l->line_buf->b, &color, &bold);

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
            if (linenoise_ctx->options.free_hints_callback)
            {
                linenoise_ctx->options.free_hints_callback(hint);
            }
        }
    }
}

/* Register a hits function to be called to show hints to the user at the
 * right of the cursor. */
void
linenoise_set_hints_callback(
    linenoise_st * const linenoise_ctx,
    linenoise_hints_callback * const fn)
{
    linenoise_ctx->options.hints_callback = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoise_set_hints_callback(). */
void
linenoise_set_free_hints_callback(
    linenoise_st * const linenoise_ctx,
    linenoise_free_hints_callback * const fn)
{
    linenoise_ctx->options.free_hints_callback = fn;
}

#endif

