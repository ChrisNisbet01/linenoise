#include "linenoise.h"
#include "linenoise_private.h"


#if WITH_KEY_BINDING
#include <string.h>

void
linenoise_bind_key(
    linenoise_st * const linenoise_ctx,
    uint8_t const key,
    key_binding_handler_cb const handler,
    void * const user_ctx)
{
    linenoise_ctx->key_bindings[key].handler = handler;
    linenoise_ctx->key_bindings[key].user_ctx = user_ctx;
}

void
linenoise_delete_text(
    linenoise_st * const linenoise_ctx,
    unsigned const start,
    unsigned const end)
{
    if (end == start)
    {
        return;
    }
    struct linenoise_state * const ls = &linenoise_ctx->state;

    /* move any text which is left, including terminator */
    unsigned const delta = end - start;
    char * line = linenoise_line_get(linenoise_ctx);
    memmove(&line[start], &line[start + delta], ls->len + 1 - end);
    ls->len -= delta;

    /* now adjust the indexes */
    if (ls->pos > end)
    {
        /* move the insertion point back appropriately */
        ls->pos -= delta;
    }
    else if (ls->pos > start)
    {
        /* move the insertion point to the start */
        ls->pos = start;
    }
}

/*
 * Insert text into the line at the current cursor position.
 */
bool
linenoise_insert_text_len(
    linenoise_st * const linenoise_ctx,
    char const * const text,
    unsigned const count)
{
    for (size_t i = 0; i < count; i++)
    {
        linenoise_edit_insert(linenoise_ctx, &linenoise_ctx->state, text[i]);
    }

    return true;
}

bool
linenoise_insert_text(
    linenoise_st * const linenoise_ctx,
    char const * const text)
{
    return linenoise_insert_text_len(linenoise_ctx, text, strlen(text));
}

void
linenoise_display_matches(
    linenoise_st * const linenoise_ctx,
    char * * const matches)
{
    char * * m;
    size_t max;
    size_t c;

    /* Find maximum completion length */
    max = 0;
    for (m = matches; *m != NULL; m++)
    {
        size_t const size = strlen(*m);

        if (max < size)
        {
            max = size;
        }
    }

    /* allow for a space between words */
    size_t const num_cols = linenoise_get_terminal_width(
        linenoise_ctx->in.fd,
        linenoise_ctx->out.fd) / (max + 1);

    /* print out a table of completions */
    fprintf(linenoise_ctx->out.stream, "\r\n");
    m = matches;
    for (m = matches; *m != NULL;)
    {
        for (c = 0; c < num_cols && *m; c++, m++)
        {
            fprintf(linenoise_ctx->out.stream, "%-*s ", (int)max, *m);
        }
        fprintf(linenoise_ctx->out.stream, "\r\n");
    }
}

bool
linenoise_complete(
    linenoise_st * const linenoise_ctx,
    unsigned const start,
    char * * const matches,
    bool const allow_prefix)
{
    bool did_some_completion;
    bool prefix;
    bool res = false;

    if (matches == NULL || matches[0] == NULL)
    {
        return false;
    }

    /* identify common prefix */
    unsigned len = strlen(matches[0]);
    prefix = true;
    for (size_t i = 1; matches[i] != NULL; i++)
    {
        unsigned common;

        for (common = 0; common < len; common++)
        {
            if (matches[0][common] != matches[i][common])
            {
                break;
            }
        }
        if (len != common)
        {
            len = common;
            prefix = !matches[i][len];
        }
    }

    unsigned start_from = 0;
    unsigned const end = linenoise_point_get(linenoise_ctx);

    /*
     * The portion of the match from the start to the cursor position
     * matches so it's only necessary to insert from that position now.
     * Exclude the characters that already match.
     */
    start_from = end - start;
    len -= end - start;

    /* Insert the rest of the common prefix */

    if (len > 0)
    {
        if (!linenoise_insert_text_len(linenoise_ctx, &matches[0][start_from], len))
        {
            return false;
        }
        did_some_completion = true;
    }
    else
    {
        did_some_completion = false;
    }

    /* is there only one completion? */
    if (!matches[1])
    {
        res = true;
        goto done;
    }

    /* is the prefix valid? */
    if (prefix && allow_prefix)
    {
        res = true;
        goto done;
    }

    /* display matches if no progress was made */
    if (!did_some_completion)
    {
        linenoise_display_matches(linenoise_ctx, matches);
        refresh_line_check_row_clear(linenoise_ctx, &linenoise_ctx->state, false);
    }

done:

    return res;
}

#endif /* WITH_KEY_BINDING */

