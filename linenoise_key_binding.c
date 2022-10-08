#include "linenoise.h"
#include "linenoise_private.h"


#ifdef WITH_KEY_BINDING
#include <string.h>

void linenoise_bind_key(linenoise_st * const linenoise_ctx,
                        uint8_t const key,
                        key_binding_handler_cb const handler,
                        void * const user_ctx)
{
    linenoise_ctx->key_bindings[key].handler = handler;
    linenoise_ctx->key_bindings[key].user_ctx = user_ctx;
}

void linenoise_delete_text(
    linenoise_st * const linenoise_ctx, unsigned start, unsigned end)
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
bool linenoise_insert_text_len(linenoise_st * const linenoise_ctx,
                               const char * text,
                               unsigned delta)
{
    struct linenoiseState * const ls = &linenoise_ctx->state;
    char * line = linenoise_line_get(linenoise_ctx);

    if ((ls->len + delta) >= ls->buflen)
    {
        // TODO: Grow the buffer.
        return false;
    }

	if (ls->pos < ls->len) {
		/* move the current text to the right (including the terminator) */
		memmove(&line[ls->pos + delta], &line[ls->pos], (ls->len - ls->pos) + 1);
	} else {
		/* terminate the string */
		line[ls->len + delta] = '\0';
	}

	/* insert the new text */
	memcpy(&line[ls->pos], text, delta);

	/* now update the indexes */
	ls->pos += delta;
	ls->len += delta;

	return true;
}

bool linenoise_insert_text(linenoise_st * const linenoise_ctx, const char *text)
{
	bool const res = linenoise_insert_text_len(linenoise_ctx, text, strlen(text));

    refreshLine(linenoise_ctx, &linenoise_ctx->state);

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
	const char *line;
	unsigned end, len;
	bool completion;
	bool prefix;
	int i;

    if (!matches || !matches[0])
    {
		return false;
    }

	/* identify common prefix */
	len = strlen(matches[0]);
	prefix = true;
	for (i = 1; matches[i]; i++)
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
	line = linenoise_line_get(linenoise_ctx);
	end = linenoise_point_get(linenoise_ctx);
	if ((end - start) < len
	    || strncmp(line + start, matches[0], len) != 0)
    {
		linenoise_delete_text(linenoise_ctx, start, end);
        if (!linenoise_insert_text_len(linenoise_ctx, matches[0], len))
        {
			return false;
        }
        refreshLine(linenoise_ctx, &linenoise_ctx->state);
		completion = true;
	} else {
		completion = false;
	}

	/* is there only one completion? */
    if (!matches[1])
    {
		return true;
    }

	/* is the prefix valid? */
    if (prefix && allow_prefix)
    {
		return true;
    }

	/* display matches if no progress was made */
	if (!completion) {
        display_matches(linenoise_ctx, matches);
        refreshLine(linenoise_ctx, &linenoise_ctx->state);
	}

	return false;
}

#else /* WITH_KEY_BINDING */

void linenoise_bind_key(linenoise_st * const linenoise_ctx,
                        uint8_t const key,
                        key_binding_handler_cb const handler,
                        void * const user_ctx)
{
}

void linenoise_delete_text(
    linenoise_st * const linenoise_ctx, unsigned start, unsigned end)
{
}

/*
 * Insert text into the line at the current cursor position.
 */
bool linenoise_insert_text_len(linenoise_st * const linenoise_ctx,
                               const char * text,
                               unsigned delta)
{
    return false;
}

bool linenoise_insert_text(linenoise_st * const linenoise_ctx, const char *text)
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

