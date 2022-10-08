#include "linenoise.h"
#include "linenoise_private.h"


#ifdef WITH_KEY_BINDING
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
    unsigned start,
    unsigned end)
{
	unsigned delta;

    if (end == start)
    {
		return;
    }
    struct linenoiseState * const ls = &linenoise_ctx->state;

	/* move any text which is left, including terminator */
	delta = end - start;
    char * line = linenoise_line_get(linenoise_ctx);
	memmove(&line[start], &line[start + delta], ls->len + 1 - end);
	ls->len -= delta;

	/* now adjust the indexes */
	if (ls->pos > end) {
		/* move the insertion point back appropriately */
		ls->pos -= delta;
	} else if (ls->pos > start) {
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
        linenoiseEditInsert(linenoise_ctx, &linenoise_ctx->state, text[i]);
    }

    return true;
}

bool
linenoise_insert_text(
    linenoise_st * const linenoise_ctx,
    char const * const text)
{
	bool const res = linenoise_insert_text_len(linenoise_ctx, text, strlen(text));

    return res;
}

static void
display_matches(
    linenoise_st * const linenoise_ctx,
    char * * matches)
{
    char *const *m;
    size_t max;
    size_t c, cols;

    /* find maximum completion length */
    max = 0;
    for (m = matches; *m; m++) {
        size_t size = strlen(*m);
        if (max < size)
            max = size;
    }

    /* allow for a space between words */
    cols = getColumns(linenoise_ctx->in.fd, linenoise_ctx->out.fd) / (max + 1);

    /* print out a table of completions */
    fprintf(linenoise_ctx->out.stream, "\r\n");
    m = matches;
    for (m = matches; *m; ) {
        for (c = 0; c < cols && *m; c++, m++)
            fprintf(linenoise_ctx->out.stream, "%-*s ", (int)max, *m);
        fprintf(linenoise_ctx->out.stream, "\r\n");
    }
}

bool
linenoise_complete(
    linenoise_st * const linenoise_ctx,
    unsigned start,
	char **matches,
    bool allow_prefix)
{
/*
 * I don't think there's ever a time when deleting part of the line is
 * required.
 */
#define DELETE_MISMATCHED_PREFIX 0
#if DELETE_MISMATCHED_PREFIX
	const char *line;
#endif
	unsigned end, len;
	bool did_some_completion;
	bool prefix;
	int i;
    bool res = false;

    if (matches == NULL || matches[0] == NULL)
    {
		return false;
    }

	/* identify common prefix */
	len = strlen(matches[0]);
	prefix = true;
	for (i = 1; matches[i] != NULL; i++)
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

	/* insert common prefix */
#if DELETE_MISMATCHED_PREFIX
	line = linenoise_line_get(linenoise_ctx);
#endif
	end = linenoise_point_get(linenoise_ctx);
    unsigned start_from = 0;
    bool must_refresh = false;

#if DELETE_MISMATCHED_PREFIX
    /*
     * Only delete chars if there is a mismatch between line and the common
     * match prefix.
     */
    if (strncmp(line + start, matches[0], (end - start)) != 0)
    {
		linenoise_delete_text(linenoise_ctx, start, end);
        must_refresh = true;
	}
    else
#endif
    {
        /*
         * The portion of the match from the start to the cursor position
         * matches so it's only necessary to insert from that position now.
         * Exclude the characters that already match.
         */
        start_from = end - start;
        len -= end - start;
    }

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
        display_matches(linenoise_ctx, matches);
        must_refresh = true;
	}

done:
    if (must_refresh)
    {
        refreshLine(linenoise_ctx, &linenoise_ctx->state);
    }

	return res;
}

#else /* WITH_KEY_BINDING */
/*
 * Just include some stubs so that users can link against a lib without this
 * support.
 */
void
linenoise_bind_key(
    linenoise_st * const linenoise_ctx,
    uint8_t const key,
    key_binding_handler_cb const handler,
    void * const user_ctx)
{
}

void
linenoise_delete_text(
    linenoise_st * const linenoise_ctx,
    unsigned start,
    unsigned end)
{
}

/*
 * Insert text into the line at the current cursor position.
 */
bool
linenoise_insert_text_len(
    linenoise_st * const linenoise_ctx,
    const char * text,
    unsigned delta)
{
    return false;
}

bool
linenoise_insert_text(
    linenoise_st * const linenoise_ctx,
    const char *text)
{
    return false;
}

bool
linenoise_complete(
    linenoise_st * const linenoise_ctx,
    unsigned start,
	char **matches,
    bool allow_prefix)
{
	return false;
}

#endif /* WITH_KEY_BINDING */

