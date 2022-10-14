#include "linenoise.h"
#include "linenoise_private.h"
#include "export.h"

#include <string.h>
#include <stdlib.h>

NO_EXPORT
struct linenoise_keymap *
linenoise_keymap_new(void)
{
    struct linenoise_keymap * const keymap = calloc(1, sizeof(*keymap));

    return keymap;
}

NO_EXPORT
void
linenoise_keymap_free(struct linenoise_keymap * const keymap)
{
    for (size_t i = 0; i < KEYMAP_SIZE; i++)
    {
        if (keymap->key[i].keymap != NULL)
        {
            linenoise_keymap_free(keymap->key[i].keymap);
        }
    }
    free(keymap);
}

void
linenoise_bind_keyseq(
    linenoise_st * const linenoise_ctx,
    const char * const seq_in,
    linenoise_key_binding_handler_cb const handler,
    void * const context)
{
	struct linenoise_keymap * keymap;
	unsigned char key;
    const char * seq = seq_in;

    if (seq[0] == '\0')
    {
		return;
    }

	keymap = linenoise_ctx->keymap;
    key = seq[0];
    seq++;

	while (seq[0] != '\0')
    {
        if (keymap->key[key].keymap == NULL)
        {
			keymap->key[key].keymap = linenoise_keymap_new();
        }
		keymap = keymap->key[key].keymap;
        key = seq[0];
        seq++;
	}

	keymap->key[key].handler = handler;
	keymap->key[key].context = context;
}

void
linenoise_bind_key(
    linenoise_st * const linenoise_ctx,
    uint8_t const key,
    linenoise_key_binding_handler_cb const handler,
    void * const user_ctx)
{
    char seq[2] = {key, '\0'};

    linenoise_bind_keyseq(linenoise_ctx, seq, handler, user_ctx);
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
    char * const line = linenoise_line_get(linenoise_ctx);
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
    uint32_t flags = 0;

    for (size_t i = 0; i < count; i++)
    {
        linenoise_edit_insert(linenoise_ctx, &flags, text[i]);
    }

    if ((flags & linenoise_key_handler_refresh) != 0)
    {
        linenoise_refresh_line(linenoise_ctx);
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
    size_t max;

    /* Find maximum completion length */
    max = 0;
    for (char * * m = matches; *m != NULL; m++)
    {
        size_t const size = strlen(*m);

        if (max < size)
        {
            max = size;
        }
    }

    /* allow for a space between words */
    size_t const num_cols = linenoise_terminal_width(linenoise_ctx) / (max + 1);

    /* print out a table of completions */
    fprintf(linenoise_ctx->out.stream, "\r\n");
    for (char * * m = matches; *m != NULL;)
    {
        for (size_t c = 0; c < num_cols && *m; c++, m++)
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
        refresh_multi_line(linenoise_ctx, false);
    }

done:

    return res;
}

