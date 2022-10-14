/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2016, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#include "linenoise.h"
#include "linenoise_private.h"
#include "buffer.h"
#include "export.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>


#define DEFAULT_TERMINAL_WIDTH 80
#define ESCAPESTR "\x1b"

static char const * const unsupported_term[] = { "dumb", "cons25", "emacs", NULL };

enum KEY_ACTION
{
    KEY_NULL = 0,       /* NULL */
    CTRL_A = 1,         /* Ctrl+a */
    CTRL_B = 2,         /* Ctrl-b */
    CTRL_C = 3,         /* Ctrl-c */
    CTRL_D = 4,         /* Ctrl-d */
    CTRL_E = 5,         /* Ctrl-e */
    CTRL_F = 6,         /* Ctrl-f */
    CTRL_H = 8,         /* Ctrl-h */
    TAB = 9,            /* Tab */
    CTRL_K = 11,        /* Ctrl+k */
    CTRL_L = 12,        /* Ctrl+l */
    ENTER = 13,         /* Enter */
    CTRL_N = 14,        /* Ctrl-n */
    CTRL_P = 16,        /* Ctrl-p */
    CTRL_T = 20,        /* Ctrl-t */
    CTRL_U = 21,        /* Ctrl+u */
    CTRL_W = 23,        /* Ctrl+w */
    ESC = 27,           /* Escape */
    BACKSPACE =  127    /* Backspace */
};

char *
linenoise_line_get(linenoise_st * const linenoise_ctx)
{
    return linenoise_ctx->state.line_buf->b;
}

size_t
linenoise_point_get(linenoise_st * const linenoise_ctx)
{
    return linenoise_ctx->state.pos;
}

size_t
linenoise_end_get(linenoise_st * const linenoise_ctx)
{
    return linenoise_ctx->state.len;
}

void
linenoise_point_set(
    linenoise_st * const linenoise_ctx,
    unsigned const new_point)
{
    linenoise_ctx->state.pos = new_point;
}

/* ======================= Low level terminal handling ====================== */

/* Enable or disable "mask mode". When it is enabled, instead of the input that
 * the user is typing, the terminal will just display a corresponding
 * number of asterisks, like "****". This is useful for passwords and other
 * secrets that should not be displayed. */
void
linenoise_set_mask_mode(
    linenoise_st * const linenoise_ctx, bool const enable)
{
    linenoise_ctx->options.mask_mode = enable;
}

/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
static int
is_unsupported_terminal(void)
{
    char * term = getenv("TERM");
    int j;

    if (term == NULL)
    {
        return 0;
    }
    for (j = 0; unsupported_term[j]; j++)
    {
        if (strcasecmp(term, unsupported_term[j]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Raw mode: 1960 magic shit. */
static int
enable_raw_mode(linenoise_st * const linenoise_ctx, int const fd)
{
    if (!isatty(fd))
    {
        goto fatal;
    }

    if (tcgetattr(fd, &linenoise_ctx->orig_termios) == -1)
    {
        goto fatal;
    }

    struct termios raw;

    raw = linenoise_ctx->orig_termios;  /* modify the original mode */
    raw.c_iflag = 0;
    raw.c_oflag = OPOST | ONLCR;
    raw.c_lflag = 0;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
    {
        goto fatal;
    }

    linenoise_ctx->in_raw_mode = true;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void
disable_raw_mode(linenoise_st * const linenoise_ctx, int const fd)
{
    if (linenoise_ctx->in_raw_mode
        && tcsetattr(fd, TCSAFLUSH, &linenoise_ctx->orig_termios) != -1)
    {
        linenoise_ctx->in_raw_mode = false;
    }
}

/*
 * Try to get the number of columns in the current terminal, or assume 80
 * if it fails.*
 */
int
linenoise_terminal_width(linenoise_st * const linenoise_ctx)
{
    int cols = DEFAULT_TERMINAL_WIDTH;
    struct winsize ws;

    if (ioctl(linenoise_ctx->out.fd, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0)
    {
        cols = ws.ws_col;
    }

    return cols;
}

/* Clear the screen. Used to handle ctrl+l */
void
linenoise_clear_screen(linenoise_st * const linenoise_ctx)
{
    if (write(linenoise_ctx->out.fd, "\x1b[H\x1b[2J", 7) <= 0)
    {
        /* nothing to do, just to avoid warning. */
    }
}

NO_EXPORT
/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
bool
refresh_multi_line(
    linenoise_st * const linenoise_ctx,
    bool const row_clear_required)
{
    struct linenoise_state * const l = &linenoise_ctx->state;
    bool success = true;
    char seq[64];
    int plen = strlen(l->prompt);
    int rows = (plen + l->len + l->cols - 1) / l->cols; /* rows used by current buf. */
    int rpos = (plen + l->oldpos + l->cols) / l->cols; /* cursor relative row. */
    int rpos2; /* rpos after refresh. */
    int col; /* column position, zero-based. */
    int old_rows = l->maxrows;
    int const fd = linenoise_ctx->out.fd;
    struct buffer ab;

    /* Update maxrows if needed. */
    if (rows > (int)l->maxrows)
    {
        l->maxrows = rows;
    }

    linenoise_buffer_init(&ab, 20);
    /*
     * First step: clear all the lines used before.
     * To do so start by going to the last row.
     * This isn't necessary if there have been some completions printed just
     * before this function is called, because the cursor will already be at
     * the start of a line. In that case, row_clear_required will be false.
     */
    if (row_clear_required)
    {
        if (old_rows - rpos > 0)
        {
            linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\x1b[%dB", old_rows - rpos);
        }

        /* Now for every row clear it, go up. */
        for (int j = 0; j < old_rows - 1; j++)
        {
            linenoise_buffer_append(&ab, "\r\x1b[0K\x1b[1A", strlen("\r\x1b[0K\x1b[1A"));
        }

        /* Clean the top line. */
        linenoise_buffer_append(&ab, "\r\x1b[0K", strlen("\r\x1b[0K"));
    }

    /* Write the prompt and the current buffer content */
    linenoise_buffer_append(&ab, l->prompt, strlen(l->prompt));
    if (linenoise_ctx->options.mask_mode)
    {
        for (size_t i = 0; i < l->len; i++)
        {
            linenoise_buffer_append(&ab, "*", 1);
        }
    }
    else
    {
        linenoise_buffer_append(&ab, l->line_buf->b, l->len);
    }

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (l->pos && l->pos == l->len && ((l->pos + plen) % l->cols) == 0)
    {
        linenoise_buffer_append(&ab, "\n\r", strlen("\n\r"));
        rows++;
        if (rows > (int)l->maxrows)
        {
            l->maxrows = rows;
        }
    }

    /* Move cursor to right position. */
    rpos2 = (plen + l->pos + l->cols) / l->cols; /* current cursor relative row. */

    /* Go up till we reach the expected positon. */
    if (rows - rpos2 > 0)
    {
        linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\x1b[%dA", rows - rpos2);
    }

    /* Set column. */
    col = (plen + (int)l->pos) % (int)l->cols;
    if (col != 0)
    {
        linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\r\x1b[%dC", col);
    }
    else
    {
        linenoise_buffer_append(&ab, "\r", strlen("\r"));
    }

    l->oldpos = l->pos;

    if (write(fd, ab.b, ab.len) == -1)
    {
        success = false;
    }
    linenoise_buffer_free(&ab);

    return success;
}

bool
linenoise_refresh_line(linenoise_st *linenoise_ctx)
{
    return refresh_multi_line(linenoise_ctx, true);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
NO_EXPORT
int
linenoise_edit_insert(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
    uint32_t * const flags,
    char const c)
{
    if (l->len >= l->line_buf->capacity)
    {
        if (!linenoise_buffer_grow(l->line_buf, l->len - l->line_buf->capacity))
        {
            goto done;
        }
    }

    bool require_full_refresh = true;

    if (l->len == l->pos) /* Cursor is at the end of the line. */
    {
        size_t const old_rows = (l->prompt_len + l->len) / l->cols;
        size_t const new_rows = (l->prompt_len + l->len + 1) / l->cols;

        if (old_rows == new_rows)
        {
            require_full_refresh = false;
        }
    }

    /* Insert the new char into the line buffer. */
    if (l->len != l->pos)
    {
        memmove(l->line_buf->b + l->pos + 1, l->line_buf->b + l->pos, l->len - l->pos);
    }
    l->line_buf->b[l->pos] = c;
    l->len++;
    l->pos++;
    l->line_buf->b[l->len] = '\0';

    if (require_full_refresh)
    {
        *flags |= key_binding_refresh;
    }
    else
    {
        /* Avoid a full update of the line in the trivial case. */
        char const d = linenoise_ctx->options.mask_mode ? '*' : c;

        if (write(linenoise_ctx->out.fd, &d, 1) == -1)
        {
            return -1;
        }
    }

done:
    return 0;
}

/* Move cursor to the end of the line. */
static bool
linenoise_edit_move_end(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos != l->len)
    {
        l->pos = l->len;
        return true;
    }

    return false;
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
enum linenoise_history_direction
{
    LINENOISE_HISTORY_NEXT = 0,
    LINENOISE_HISTORY_PREV = 1
};

static bool
linenoise_edit_history_next(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
    enum linenoise_history_direction const dir)
{
    if (linenoise_ctx->history.current_len > 1)
    {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        free(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index]);
        linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index] = strdup(l->line_buf->b);
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0)
        {
            l->history_index = 0;
            return false;
        }
        else if (l->history_index >= linenoise_ctx->history.current_len)
        {
            l->history_index = linenoise_ctx->history.current_len - 1;
            return false;
        }
        linenoise_buffer_free(l->line_buf);
        linenoise_buffer_init(l->line_buf,
                              strlen(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index]));
        linenoise_buffer_append(l->line_buf,
                                linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index],
                                strlen(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index]));
        l->len = l->pos = l->line_buf->len;
        return true;
    }
    return false;
}

/*
 * Delete the character at the right of the cursor without altering the cursor
 * position.
 * Basically this is what happens with the "Delete" keyboard key.
 */
static bool
linenoise_edit_delete(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->len > 0 && l->pos < l->len)
    {
        memmove(l->line_buf->b + l->pos, l->line_buf->b + l->pos + 1, l->len - l->pos - 1);
        l->len--;
        l->line_buf->b[l->len] = '\0';
        return true;
    }

    return false;
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. */
static void
linenoise_edit_delete_prev_word(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->line_buf->b[l->pos - 1] == ' ')
    {
        l->pos--;
    }
    while (l->pos > 0 && l->line_buf->b[l->pos - 1] != ' ')
    {
        l->pos--;
    }
    diff = old_pos - l->pos;
    memmove(l->line_buf->b + l->pos,
            l->line_buf->b + old_pos,
            l->len - old_pos + 1);
    l->len -= diff;
}

static void
delete_whole_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    l->line_buf->b[0] = '\0';
    l->pos = 0;
    l->len = 0;
}

static void
linenoise_edit_done(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    struct linenoise_state * const l)
{
    linenoise_ctx->history.current_len--;
    free(linenoise_ctx->history.history[linenoise_ctx->history.current_len]);
    linenoise_ctx->history.history[linenoise_ctx->history.current_len] = NULL;
    linenoise_edit_move_end(linenoise_ctx, l);
}

static int
linenoise_getchar_nonblock(int const fd, char * const key)
{
	int const flags = fcntl(fd, F_GETFL, 0);

    if (flags != -1)
    {
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    int const nread = read(fd, key, 1);
    if (flags != -1)
    {
		fcntl(fd, F_SETFL, flags);
    }
	return nread;
}

static bool
delete_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Delete the character to the right of the cursor. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (linenoise_edit_delete(linenoise_ctx, l))
    {
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
up_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Show the previous history entry. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_PREV))
    {
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
down_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Show the next history entry. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_NEXT))
    {
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
right_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Move the cursor right one position. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (l->pos != l->len)
    {
        l->pos++;
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
left_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Move the cursor left one position. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (l->pos > 0)
    {
        l->pos--;
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
home_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Move the cursor to the start of the line. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (l->pos != 0)
    {
        l->pos = 0;
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
end_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Move the cursor to the EOL. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (linenoise_edit_move_end(linenoise_ctx, l))
    {
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
default_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Insert the key at the current cursor position. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (linenoise_edit_insert(linenoise_ctx, l, flags, *key) != 0)
    {
        *flags |= key_binding_error;
    }
    return true;
}


static bool
enter_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Indicate that processing is done. */
    *flags |= key_binding_done;

    return true;
}

static bool
ctrl_c_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Clear the whole line and indicate that processing is done. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    delete_whole_line(linenoise_ctx, l);
    *flags |= key_binding_done;

    return true;
}

static bool
backspace_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Delete the character to the left of the cursor. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (l->pos > 0 && l->len > 0)
    {
        memmove(l->line_buf->b + l->pos - 1,
                l->line_buf->b + l->pos,
                l->len - l->pos);
        l->pos--;
        l->len--;
        l->line_buf->b[l->len] = '\0';
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
ctrl_d_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /*
     * Delete the character to the right of the cursor if there is one,
     * else indicate EOF (i.e. results in an error and program typically exits).
     */
    struct linenoise_state * const l = &linenoise_ctx->state;
    bool result;

    if (l->len > 0)
    {
        result = delete_handler(linenoise_ctx, flags, key, user_ctx);
    }
    else
    {
        /* Line is empty, so indicate an error. */
        linenoise_ctx->history.current_len--;
        free(linenoise_ctx->history.history[linenoise_ctx->history.current_len]);
        linenoise_ctx->history.history[linenoise_ctx->history.current_len] = NULL;

        *flags |= key_binding_error;
        result = true;
    }

    return result;
}

static bool
ctrl_t_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /*
     * Swap the current character with the one to its left, and move the
     * cursor right one position.
     */
    struct linenoise_state * const l = &linenoise_ctx->state;

    if (l->pos > 0 && l->pos < l->len)
    {
        int const aux = l->line_buf->b[l->pos - 1];

        l->line_buf->b[l->pos - 1] = l->line_buf->b[l->pos];
        l->line_buf->b[l->pos] = aux;
        if (l->pos != l->len - 1)
        {
            l->pos++;
        }
        *flags |= key_binding_refresh;
    }

    return true;
}

static bool
ctrl_u_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Delete the whole line. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    delete_whole_line(linenoise_ctx, l);
    *flags |= key_binding_refresh;

    return true;
}

static bool
ctrl_k_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Delete from cursor to EOL. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    l->line_buf->b[l->pos] = '\0';
    l->len = l->pos;
    *flags |= key_binding_refresh;

    return true;
}

static bool
ctrl_l_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Clear the screen and move the cursor to EOL. */
    linenoise_clear_screen(linenoise_ctx);
    *flags |= key_binding_refresh;

    return true;
}

static bool
ctrl_w_handler(
    linenoise_st * const linenoise_ctx,
    uint32_t * const flags,
    char const * key,
    void * const user_ctx)
{
    /* Delete the previous word. */
    struct linenoise_state * const l = &linenoise_ctx->state;

    linenoise_edit_delete_prev_word(linenoise_ctx, l);
    *flags |= key_binding_refresh;

    return true;
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user types enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer. */
static int linenoise_edit(
    linenoise_st * const linenoise_ctx,
    struct buffer * const line_buf,
    char const * const prompt)
{
    memset(&linenoise_ctx->state, 0, sizeof linenoise_ctx->state);

    struct linenoise_state * const l = &linenoise_ctx->state;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l->line_buf = line_buf;
    l->prompt = prompt;
    l->prompt_len = strlen(prompt);
    l->oldpos = 0;
    l->pos = 0;
    l->len = 0;
    l->cols = linenoise_terminal_width(linenoise_ctx);
    l->maxrows = 0;
    l->history_index = 0;

    /* Buffer starts empty. */
    l->line_buf->b[0] = '\0';

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoise_history_add(linenoise_ctx, "");

    if (write(linenoise_ctx->out.fd, prompt, l->prompt_len) == -1)
    {
        return -1;
    }

    while (1)
    {
        char c;
        int nread;

        nread = read(linenoise_ctx->in.fd, &c, 1);
        if (nread <= 0)
        {
            return l->len;
        }

        {
            struct linenoise_keymap * keymap = linenoise_ctx->keymap;
            key_binding_handler_cb handler = NULL;
            void * context = NULL;

            for (;;)
            {
                uint8_t const index = c;
                if (keymap->handler[index] != NULL)
                {
                    /* Indicates the end a sequence*/
                    handler = keymap->handler[index];
                    context = keymap->context[index];
                    break;
                }
                keymap = keymap->keymap[index];
                if (keymap == NULL)
                {
                    break;
                }

                char new_c;
                nread = linenoise_getchar_nonblock(linenoise_ctx->in.fd, &new_c);
                if (nread <= 0)
                {
                    break;
                }
                c = new_c;
            }

            if (handler != NULL)
            {
                char key_str[2] = { c, '\0' };
                uint32_t flags = 0;
                bool const res = handler(linenoise_ctx, &flags, key_str, context);
                (void)res;

                if ((flags & key_binding_error) != 0)
                {
                    return -1;
                }
                if ((flags & key_binding_refresh) != 0)
                {
                    linenoise_refresh_line(linenoise_ctx);
                }
                if ((flags & key_binding_done) != 0)
                {
                    linenoise_edit_done(linenoise_ctx, &flags, l);
                    break;
                }
                continue;
            }
        }
    }
    return l->len;
}

#define LINENOISE_PRINT_KEY_CODES_SUPPORT 0
#if LINENOISE_PRINT_KEY_CODES_SUPPORT
/* This special mode is used by linenoise in order to print scan codes
 * on screen for debugging / development purposes. It is implemented
 * by the linenoise_example program using the --keycodes option. */
void
linenoise_print_key_codes(linenoise_st * const linenoise_ctx)
{
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
           "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (enable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd) == -1)
    {
        return;
    }
    memset(quit, ' ', 4);
    while (1)
    {
        char c;
        int nread;

        nread = read(linenoise_ctx->in.fd, &c, 1);
        if (nread <= 0)
        {
            continue;
        }
        memmove(quit, quit + 1, sizeof(quit) - 1); /* shift string to left. */
        quit[sizeof(quit) - 1] = c; /* Insert current char on the right. */
        if (memcmp(quit, "quit", sizeof(quit)) == 0)
        {
            break;
        }

        printf("'%c' %02x (%d) (type quit to exit)\n",
               isprint(c) ? c : '?', (int)c, (int)c);
        printf("\r"); /* Go left edge manually, we are in raw mode. */
        fflush(linenoise_ctx->out.stream);
    }
    disable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd);
}
#endif

/* This function calls the line editing function linenoiseEdit() using
 * the in_fd file descriptor set in raw mode. */
static int
linenoise_raw(
    linenoise_st * const linenoise_ctx,
    struct buffer * const line_buf,
    char const * const prompt)
{
    int count;

    if (enable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd) == -1)
    {
        return -1;
    }
    count = linenoise_edit(linenoise_ctx, line_buf, prompt);
    disable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd);

    return count;
}

/* This function is called when linenoise() is called with the standard
 * input file descriptor not attached to a TTY. So for example when the
 * program using linenoise is called in pipe or with a file redirected
 * to its standard input. In this case, we want to be able to return the
 * line regardless of its length (by default we are limited to 4k). */
static char *
linenoise_no_tty(linenoise_st * const linenoise_ctx)
{
    char * line = NULL;
    size_t len = 0;
    size_t maxlen = 0;

    while (1)
    {
        /*
         * Grow the buffer.
         * XXX - Use append buffer?
         */
        if (len == maxlen)
        {
            if (maxlen == 0)
            {
                maxlen = 16;
            }
            maxlen *= 2;
            char * const oldval = line;
            line = realloc(line, maxlen + 1);
            if (line == NULL)
            {
                if (oldval)
                {
                    free(oldval);
                }
                return NULL;
            }
            line[len] = '\0';
        }

        int c = fgetc(linenoise_ctx->in.stream);
        if (c == EOF || c == '\n')
        {
            if (c == EOF && len == 0)
            {
                free(line);
                return NULL;
            }
            else
            {
                return line;
            }
        }
        else
        {
            line[len] = c;
            len++;
            line[len] = '\0';
        }
    }
    /* Unreachable */
    return NULL;
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions. */
char *
linenoise(linenoise_st * const linenoise_ctx, char const * prompt)
{
    char * line;

    if (!linenoise_ctx->is_a_tty)
    {
        /* Not a tty: read from file / pipe. In this mode we don't want any
         * limit to the line size, so we call a function to handle that. */
        line = linenoise_no_tty(linenoise_ctx);
    }
    else if (is_unsupported_terminal())
    {
        size_t len;
        struct buffer line_buf;

        linenoise_buffer_init(&line_buf, LINENOISE_MAX_LINE);

        fprintf(linenoise_ctx->out.stream, "%s", prompt);
        fflush(linenoise_ctx->out.stream);

        if (fgets(line_buf.b, line_buf.capacity, linenoise_ctx->in.stream) == NULL)
        {
            linenoise_buffer_free(&line_buf);
            line = NULL;
            goto done;
        }
        len = strlen(line_buf.b);
        while (len && (line_buf.b[len - 1] == '\n' || line_buf.b[len - 1] == '\r'))
        {
            len--;
            line_buf.b[len] = '\0';
        }
        /*
         * Ensure that the buffer is freed _after_ the buffer is duplicated so
         * the memory being duplicated is still valid.
         */
        line = strdup(line_buf.b);
        linenoise_buffer_free(&line_buf);
    }
    else
    {
        struct buffer line_buf;

        linenoise_buffer_init(&line_buf, 0);

        int const count = linenoise_raw(linenoise_ctx, &line_buf, prompt);

        if (count == -1)
        {
            line = NULL;
        }
        else
        {
            line = strdup(line_buf.b);
        }

        linenoise_buffer_free(&line_buf);
    }

done:
    if (line == NULL || line[0] == '\0')
    {
        /*
         * Without this, when empty lines (e.g. after CTRL-C) are returned,
         * the next prompt gets written out on the same line as the previous.
         */
        write(linenoise_ctx->out.fd, "\n", 1);
    }
    return line;
}

/* This is just a wrapper the user may want to call in order to make sure
 * the linenoise returned buffer is freed with the same allocator it was
 * created with. Useful when the main program is using an alternative
 * allocator. */
void
linenoise_free(void * const ptr)
{
    free(ptr);
}

/* ================================ History ================================= */

/* Free the history, but does not reset it. Only used when we have to
 * exit() to avoid memory leaks are reported by valgrind & co. */
static void
free_history(linenoise_st * const linenoise_ctx)
{
    if (linenoise_ctx->history.history != NULL)
    {
        for (int j = 0; j < linenoise_ctx->history.current_len; j++)
        {
            free(linenoise_ctx->history.history[j]);
        }
        free(linenoise_ctx->history.history);
    }
}

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
int
linenoise_history_add(linenoise_st * const linenoise_ctx, char const * const line)
{
    if (linenoise_ctx->history.max_len == 0)
    {
        return 0;
    }

    /* Initialization on first call. */
    if (linenoise_ctx->history.history == NULL)
    {
        linenoise_ctx->history.history =
            calloc(sizeof(*linenoise_ctx->history.history),
                   linenoise_ctx->history.max_len);
        if (linenoise_ctx->history.history == NULL)
        {
            return 0;
        }
    }

    /* Don't add duplicated lines. */
    if (linenoise_ctx->history.current_len
        && strcmp(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1], line) == 0)
    {
        return 0;
    }

    /*
     * Add a heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line.
     */
    char * const linecopy = strdup(line);

    if (linecopy == NULL)
    {
        return 0;
    }
    if (linenoise_ctx->history.current_len == linenoise_ctx->history.max_len)
    {
        free(linenoise_ctx->history.history[0]);
        memmove(linenoise_ctx->history.history,
                linenoise_ctx->history.history + 1,
                sizeof(char *) * (linenoise_ctx->history.max_len - 1));
        linenoise_ctx->history.current_len--;
    }
    linenoise_ctx->history.history[linenoise_ctx->history.current_len] = linecopy;
    linenoise_ctx->history.current_len++;

    return 1;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int
linenoise_history_set_max_len(linenoise_st * const linenoise_ctx, int const len)
{
    if (len < 1)
    {
        return 0;
    }
    if (linenoise_ctx->history.history)
    {
        int tocopy = linenoise_ctx->history.current_len;
        char * * const new_history = calloc(sizeof(*new_history), len);

        if (new_history == NULL)
        {
            return 0;
        }

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy)
        {
            for (int j = 0; j < tocopy - len; j++)
            {
                free(linenoise_ctx->history.history[j]);
            }
            tocopy = len;
        }
        memcpy(new_history, linenoise_ctx->history.history + (linenoise_ctx->history.current_len - tocopy), sizeof(char *) * tocopy);
        free(linenoise_ctx->history.history);
        linenoise_ctx->history.history = new_history;
    }
    linenoise_ctx->history.max_len = len;
    if (linenoise_ctx->history.current_len > linenoise_ctx->history.max_len)
    {
        linenoise_ctx->history.current_len = linenoise_ctx->history.max_len;
    }

    return 1;
}

#if LINENOISE_HISTORY_FILE_SUPPORT
/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int
linenoise_history_save(linenoise_st * const linenoise_ctx, char const * filename)
{
    mode_t const old_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    FILE * const fp = fopen(filename, "w");

    umask(old_umask);
    if (fp == NULL)
    {
        return -1;
    }
    chmod(filename, S_IRUSR | S_IWUSR);
    for (size_t j = 0; j < history_len; j++)
    {
        fprintf(fp, "%s\n", linenoise_ctx->history[j]);
    }
    fclose(fp);

    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int
linenoise_history_load(linenoise_st * const linenoise_ctx, char const * filename)
{
    char buf[LINENOISE_MAX_LINE];
    FILE * const fp = fopen(filename, "r");

    if (fp == NULL)
    {
        return -1;
    }

    while (fgets(buf, sizeof buf, fp) != NULL)
    {
        char * p;

        p = strchr(buf, '\r');
        if (p == NULL)
        {
            p = strchr(buf, '\n');
        }
        if (p != NULL)
        {
            *p = '\0';
        }
        linenoise_history_add(linenoise_ctx, buf);
    }
    fclose(fp);

    return 0;
}
#endif


struct linenoise_st *
linenoise_new(FILE * const in_stream, FILE * const out_stream)
{
    linenoise_st * const linenoise_ctx = calloc(1, sizeof *linenoise_ctx);

    if (linenoise_ctx == NULL)
    {
        goto done;
    }

    linenoise_ctx->keymap = linenoise_keymap_new();

    for (size_t i = 32; i < 256; i++)
    {
        linenoise_bind_key(linenoise_ctx, i, default_handler, NULL);
    }

    linenoise_bind_key(linenoise_ctx, ENTER, enter_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_C, ctrl_c_handler, NULL);
    linenoise_bind_key(linenoise_ctx, BACKSPACE, backspace_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_H, backspace_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_D, ctrl_d_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_T, ctrl_t_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_B, left_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_F, right_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_P, up_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_N, down_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_U, ctrl_u_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_K, ctrl_k_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_A, home_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_E, end_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_L, ctrl_l_handler, NULL);
    linenoise_bind_key(linenoise_ctx, CTRL_W, ctrl_w_handler, NULL);

    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[3~", delete_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[A", up_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[B", down_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[C", right_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[D", left_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[H", home_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "[F", end_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "OH", home_handler, NULL);
    linenoise_bind_keyseq(linenoise_ctx, ESCAPESTR "OF", end_handler, NULL);


    linenoise_ctx->in.stream = in_stream;
    linenoise_ctx->in.fd = fileno(in_stream);
    linenoise_ctx->is_a_tty = isatty(linenoise_ctx->in.fd);

    linenoise_ctx->out.stream = out_stream;
    linenoise_ctx->out.fd = fileno(out_stream);

    linenoise_ctx->history.max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;

done:
    return linenoise_ctx;
}

void
linenoise_delete(linenoise_st * const linenoise_ctx)
{
    if (linenoise_ctx == NULL)
    {
        goto done;
    }

    if (linenoise_ctx->in_raw_mode)
    {
        disable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd);
    }
    linenoise_keymap_free(linenoise_ctx->keymap);
    linenoise_ctx->keymap = NULL;

    free_history(linenoise_ctx);

    free(linenoise_ctx);

done:
    return;
}

int
linenoise_printf(
    linenoise_st * const linenoise_ctx,
    char const * const fmt,
    ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vfprintf(linenoise_ctx->out.stream, fmt, args);
    va_end(args);

    return len;
}

