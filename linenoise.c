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
#if WITH_HINTS
#include "linenoise_hints.h"
#endif

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


/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)l->len,(int)l->pos,(int)l->oldpos,plen,rows,rpos, \
            (int)l->maxrows,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

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

/* Set if to use or not the multi line mode. */
void
linenoise_set_multi_line(linenoise_st * const linenoise_ctx, bool const ml)
{
    linenoise_ctx->options.multiline_mode = ml;
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
    struct termios raw;

    if (!isatty(fd))
    {
        goto fatal;
    }

    if (tcgetattr(fd, &linenoise_ctx->orig_termios) == -1)
    {
        goto fatal;
    }

    raw = linenoise_ctx->orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

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

#if QUERY_TERMINAL_FOR_WIDTH
/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int
get_cursor_position(int const ifd, int const ofd)
{
    char buf[32];
    int cols;
    int rows;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4)
    {
        return -1;
    }

    /* Read the response: ESC [ rows ; cols R */
    size_t i;
    for (i = 0; i < sizeof(buf) - 1; i++)
    {
        if (read(ifd, buf + i, 1) != 1)
        {
            break;
        }
        if (buf[i] == 'R')
        {
            break;
        }
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[')
    {
        return -1;
    }
    if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2)
    {
        return -1;
    }
    return cols;
}
#endif

/*
 * Try to get the number of columns in the current terminal, or assume 80
 * if it fails.*
 */
int
linenoise_get_terminal_width(int const ifd, int const ofd)
{
    int cols = DEFAULT_TERMINAL_WIDTH;
    struct winsize ws;

    if (ioctl(ofd, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
#if QUERY_TERMINAL_FOR_WIDTH
        /* ioctl() failed. Try to query the terminal itself. */

        /* Get the initial position so we can restore it later. */
        int const start = get_cursor_position(ifd, ofd);

        if (start == -1)
        {
            goto done;
        }

        /* Go to right margin and get position. */
        if (write(ofd, "\x1b[999C", 6) != 6)
        {
            goto done;
        }
        cols = get_cursor_position(ifd, ofd);
        if (cols == -1)
        {
            cols = DEFAULT_TERMINAL_WIDTH;
            goto done;
        }

        /* Restore position. */
        if (cols > start)
        {
            char seq[32];

            snprintf(seq, sizeof seq, "\x1b[%dD", cols - start);
            if (write(ofd, seq, strlen(seq)) == -1)
            {
                /* Can't recover... */
                cols = DEFAULT_TERMINAL_WIDTH;
                goto done;
            }
        }
#endif
    }
    else
    {
        cols = ws.ws_col;
    }

#if QUERY_TERMINAL_FOR_WIDTH
done:
#endif

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

void
linenoise_beep_control(linenoise_st * const linenoise_ctx, bool const enable)
{
    linenoise_ctx->options.disable_beep = !enable;
}

#if WITH_NATIVE_COMPLETION
/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void
linenoise_beep(linenoise_st * const linenoise_ctx)
{
    if (!linenoise_ctx->options.disable_beep)
    {
        fprintf(stderr, "\x7");
        fflush(stderr);
    }
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void
free_completions(linenoise_completions * const lc)
{
    for (size_t i = 0; i < lc->len; i++)
    {
        free(lc->cvec[i]);
    }
    if (lc->cvec != NULL)
    {
        free(lc->cvec);
    }
}

static int
print_completions(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const ls,
    linenoise_completions * const lc)
{
    int nread;
    char c = 0;

    if (lc->len == 0)
    {
        linenoise_beep(linenoise_ctx);
    }
    else
    {
        size_t stop = 0;
        size_t i = 0;

        while (!stop)
        {
            /* Show completion or original buffer */
            if (i < lc->len)
            {
                struct linenoise_state saved = *ls;
                struct buffer temp_buf;

                /* Construct the temporary buffer */
                linenoise_buffer_init(&temp_buf, strlen(lc->cvec[i]));
                linenoise_buffer_append(&temp_buf, lc->cvec[i], strlen(lc->cvec[i]));
                ls->len = ls->pos = temp_buf.len;
                ls->line_buf = &temp_buf;

                refresh_line(linenoise_ctx, ls);
                /* Restore the original line buffer. */
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->line_buf = saved.line_buf;

                linenoise_buffer_free(&temp_buf);
            }
            else
            {
                refresh_line(linenoise_ctx, ls);
            }

            nread = read(linenoise_ctx->in.fd, &c, 1);
            if (nread <= 0)
            {
                return -1;
            }

            switch (c)
            {
            case TAB: /* tab */
                i = (i + 1) % (lc->len + 1);
                if (i == lc->len)
                {
                    linenoise_beep(linenoise_ctx);
                }
                break;

            case ESC: /* escape */
                /* Re-show original buffer */
                if (i < lc->len)
                {
                    refresh_line(linenoise_ctx, ls);
                }
                stop = 1;
                break;

            default:
                /* Update buffer and return */
                if (i < lc->len)
                {
                    linenoise_buffer_free(ls->line_buf);
                    linenoise_buffer_init(ls->line_buf, strlen(lc->cvec[i]));
                    linenoise_buffer_append(ls->line_buf, lc->cvec[i], strlen(lc->cvec[i]));
                    ls->len = ls->pos = ls->line_buf->len;
                }
                stop = 1;
                break;
            }
        }
    }

    return c; /* Return last read character */
}

/* This is a helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
static int
complete_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const ls)
{
    linenoise_completions lc = { 0, NULL };

    linenoise_ctx->options.completion_callback(ls->line_buf->b, &lc);

    int const res = print_completions(linenoise_ctx, ls, &lc);

    free_completions(&lc);

    return res;
}

/* Register a callback function to be called for tab-completion. */
void
linenoise_set_completion_callback(
    linenoise_st * const linenoise_ctx,
    linenoise_completion_callback * const fn)
{
    linenoise_ctx->options.completion_callback = fn;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void
linenoise_add_completion(linenoise_completions * lc, char const * str)
{
    char * const copy = strdup(str);

    if (copy == NULL)
    {
        return;
    }

    char * * const cvec = realloc(lc->cvec, sizeof(char *) * (lc->len + 1));

    if (cvec == NULL)
    {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len] = copy;
    lc->len++;
}
#endif

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static bool
refresh_single_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
    bool const row_clear_required)
{
    bool success = true;
    char seq[64];
    size_t plen = strlen(l->prompt);
    int const fd = linenoise_ctx->out.fd;
    char * buf = l->line_buf->b;
    size_t len = l->len;
    size_t pos = l->pos;
    struct buffer ab;

    while ((plen + pos) >= l->cols)
    {
        buf++;
        len--;
        pos--;
    }
    while (plen + len > l->cols)
    {
        len--;
    }

    linenoise_buffer_init(&ab, 20);
    /* Cursor to left edge */
    linenoise_buffer_append(&ab, "\r", strlen("\r"));
    /* Write the prompt and the current buffer content */
    linenoise_buffer_append(&ab, l->prompt, strlen(l->prompt));
    if (linenoise_ctx->options.mask_mode)
    {
        while (len--)
        {
            linenoise_buffer_append(&ab, "*", 1);
        }
    }
    else
    {
        linenoise_buffer_append(&ab, buf, len);
    }
#if WITH_HINTS
    /* Show hints if any. */
    linenoise_hints_refreshShowHints(linenoise_ctx, &ab, l, plen);
#endif
    /* Erase to right */
    linenoise_buffer_append(&ab, "\x1b[0K", strlen("\x1b[0K"));
    /* Move cursor to original position. */
    linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\r\x1b[%dC",(int)(pos + plen));
    if (write(fd, ab.b, ab.len) == -1)
    {
        success = false;
    } /* Can't recover from write error. */
    linenoise_buffer_free(&ab);

    return success;
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static bool
refresh_multi_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
    bool const row_clear_required)
{
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
            lndebug("go down %d", old_rows - rpos);
            linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\x1b[%dB", old_rows - rpos);
        }

        /* Now for every row clear it, go up. */
        for (int j = 0; j < old_rows - 1; j++)
        {
            lndebug("clear+up");
            linenoise_buffer_append(&ab, "\r\x1b[0K\x1b[1A", strlen("\r\x1b[0K\x1b[1A"));
        }

        /* Clean the top line. */
        lndebug("clear");
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

#if WITH_HINTS
    /* Show hints if any. */
    linenoise_hints_refreshShowHints(linenoise_ctx, &ab, l, plen);
#endif

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (l->pos && l->pos == l->len && ((l->pos + plen) % l->cols) == 0)
    {
        lndebug("<newline>");
        linenoise_buffer_append(&ab, "\n\r", strlen("\n\r"));
        rows++;
        if (rows > (int)l->maxrows)
        {
            l->maxrows = rows;
        }
    }

    /* Move cursor to right position. */
    rpos2 = (plen + l->pos + l->cols) / l->cols; /* current cursor relative row. */
    lndebug("rpos2 %d", rpos2);

    /* Go up till we reach the expected positon. */
    if (rows - rpos2 > 0)
    {
        lndebug("go-up %d", rows - rpos2);
        linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\x1b[%dA", rows - rpos2);
    }

    /* Set column. */
    col = (plen + (int)l->pos) % (int)l->cols;
    lndebug("set col %d", 1 + col);
    if (col)
    {
        linenoise_buffer_snprintf(&ab, seq, sizeof seq, "\r\x1b[%dC", col);
    }
    else
    {
        linenoise_buffer_append(&ab, "\r", strlen("\r"));
    }

    lndebug("\n");
    l->oldpos = l->pos;

    if (write(fd, ab.b, ab.len) == -1)
    {
        success = false;
    }
    linenoise_buffer_free(&ab);

    return success;
}

/* Calls one of the two low level functions
 *      refreshSingleLine()
 * or
 *      refreshMultiLine()
 * according to the selected mode. */
NO_EXPORT
bool
refresh_line_check_row_clear(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
    bool const row_clear_required)
{
    bool success;
    if (linenoise_ctx->options.multiline_mode)
    {
        success = refresh_multi_line(linenoise_ctx, l, row_clear_required);
    }
    else
    {
        success = refresh_single_line(linenoise_ctx, l, row_clear_required);
    }

    return success;
}

NO_EXPORT
bool
refresh_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    return refresh_line_check_row_clear(linenoise_ctx, l, true);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
int
linenoise_edit_insert(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l,
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
        if (linenoise_ctx->options.multiline_mode)
        {
            size_t const old_rows = (l->prompt_len + l->len) / l->cols;
            size_t const new_rows = (l->prompt_len + l->len + 1) / l->cols;

            if (old_rows == new_rows)
            {
                require_full_refresh = false;
            }
        }
        else
        {
            if ((l->prompt_len + l->len + 1) < l->cols)
            {
                require_full_refresh = false;
            }
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
        refresh_line(linenoise_ctx, l);
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

/* Move cursor on the left. */
static void
linenoise_edit_move_left(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos > 0)
    {
        l->pos--;
        refresh_line(linenoise_ctx, l);
    }
}

/* Move cursor on the right. */
static void
linenoise_edit_move_right(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos != l->len)
    {
        l->pos++;
        refresh_line(linenoise_ctx, l);
    }
}

/* Move cursor to the start of the line. */
static void
linenoise_edit_move_home(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos != 0)
    {
        l->pos = 0;
        refresh_line(linenoise_ctx, l);
    }
}

/* Move cursor to the end of the line. */
static void
linenoise_edit_move_end(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos != l->len)
    {
        l->pos = l->len;
        refresh_line(linenoise_ctx, l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
enum linenoise_history_direction
{
    LINENOISE_HISTORY_NEXT = 0,
    LINENOISE_HISTORY_PREV = 1
};

static void
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
            return;
        }
        else if (l->history_index >= linenoise_ctx->history.current_len)
        {
            l->history_index = linenoise_ctx->history.current_len - 1;
            return;
        }
        linenoise_buffer_free(l->line_buf);
        linenoise_buffer_init(l->line_buf,
                              strlen(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index]));
        linenoise_buffer_append(l->line_buf,
                                linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index],
                                strlen(linenoise_ctx->history.history[linenoise_ctx->history.current_len - 1 - l->history_index]));
        l->len = l->pos = l->line_buf->len;
        refresh_line(linenoise_ctx, l);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
static void
linenoise_edit_delete(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->len > 0 && l->pos < l->len)
    {
        memmove(l->line_buf->b + l->pos, l->line_buf->b + l->pos + 1, l->len - l->pos - 1);
        l->len--;
        l->line_buf->b[l->len] = '\0';
        refresh_line(linenoise_ctx, l);
    }
}

/* Backspace implementation. */
static void
linenoise_edit_backspace(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    if (l->pos > 0 && l->len > 0)
    {
        memmove(l->line_buf->b + l->pos - 1,
                l->line_buf->b + l->pos,
                l->len - l->pos);
        l->pos--;
        l->len--;
        l->line_buf->b[l->len] = '\0';
        refresh_line(linenoise_ctx, l);
    }
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
    refresh_line(linenoise_ctx, l);
}

static void
delete_whole_line(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    l->line_buf->b[0] = '\0';
    l->pos = 0;
    l->len = 0;
    refresh_line(linenoise_ctx, l);
}

static void
linenoise_edit_done(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    linenoise_ctx->history.current_len--;
    free(linenoise_ctx->history.history[linenoise_ctx->history.current_len]);
    linenoise_ctx->history.history[linenoise_ctx->history.current_len] = NULL;
    if (linenoise_ctx->options.multiline_mode)
    {
        linenoise_edit_move_end(linenoise_ctx, l);
    }

#if WITH_HINTS
    if (linenoise_ctx->options.hints_callback != NULL)
    {
        /*
         * Force a refresh without hints to leave the previous
         * line as the user typed it after a newline.*
         */
        linenoise_hints_callback * const hc = linenoise_ctx->options.hints_callback;
        linenoise_ctx->options.hints_callback = NULL;
        refresh_line(linenoise_ctx, l);
        linenoise_ctx->options.hints_callback = hc;
    }
#endif

}

static int
ctrl_c_handler_default(
    linenoise_st * const linenoise_ctx,
    struct linenoise_state * const l)
{
    delete_whole_line(linenoise_ctx, l);
    linenoise_edit_done(linenoise_ctx, l);
    return (int)l->len;
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
    int const stdin_fd,
    int const stdout_fd,
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
    l->cols = linenoise_get_terminal_width(stdin_fd, stdout_fd);
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
        char seq[3];

        nread = read(linenoise_ctx->in.fd, &c, 1);
        if (nread <= 0)
        {
            return l->len;
        }


#if WITH_KEY_BINDING
        if (linenoise_ctx->key_bindings[(unsigned)c].handler != NULL)
        {
            size_t const index = (unsigned)c;
	    char key_str[2] = {c, '\0'};
	    uint32_t flags = 0;
            bool const res = linenoise_ctx->key_bindings[index].handler(
                linenoise_ctx,
		&flags,
                key_str,
                linenoise_ctx->key_bindings[index].user_ctx);
	    if (!res)
	    {
		    return -1;
	    }
	    if ((flags & key_binding_done) != 0)
	    {
		    break;
	    }
	    if ((flags & key_binding_refresh) != 0)
	    {
			refresh_line(linenoise_ctx, l);
	    }
            continue;
        }
#endif

#if WITH_NATIVE_COMPLETION
        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == TAB && linenoise_ctx->options.completion_callback != NULL)
        {
            c = complete_line(linenoise_ctx, l);
            /* Return on errors */
            if (c < 0)
            {
                return l->len;
            }
            /* Read next character when 0 */
            if (c == 0)
            {
                continue;
            }
        }
#endif

        switch (c)
        {
        case ENTER:    /* enter */
            linenoise_edit_done(linenoise_ctx, l);
            return (int)l->len;

        case CTRL_C:     /* ctrl-c */
            return ctrl_c_handler_default(linenoise_ctx, l);

        case BACKSPACE:   /* backspace */
            /* Drop through. */
        case CTRL_H:     /* ctrl-h */
            linenoise_edit_backspace(linenoise_ctx, l);
            break;

        case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
            if (l->len > 0)
            {
                linenoise_edit_delete(linenoise_ctx, l);
            }
            else
            {
                linenoise_ctx->history.current_len--;
                free(linenoise_ctx->history.history[linenoise_ctx->history.current_len]);
                linenoise_ctx->history.history[linenoise_ctx->history.current_len] = NULL;

                return -1;
            }
            break;

        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (l->pos > 0 && l->pos < l->len)
            {
                int const aux = l->line_buf->b[l->pos - 1];

                l->line_buf->b[l->pos - 1] = l->line_buf->b[l->pos];
                l->line_buf->b[l->pos] = aux;
                if (l->pos != l->len - 1)
                {
                    l->pos++;
                }
                refresh_line(linenoise_ctx, l);
            }
            break;

        case CTRL_B:     /* ctrl-b */
            linenoise_edit_move_left(linenoise_ctx, l);
            break;

        case CTRL_F:     /* ctrl-f */
            linenoise_edit_move_right(linenoise_ctx, l);
            break;

        case CTRL_P:    /* ctrl-p */
            linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_PREV);
            break;

        case CTRL_N:    /* ctrl-n */
            linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_NEXT);
            break;

        case ESC:    /* escape sequence */
            /* Read the next two bytes representing the escape sequence.
             * Use two calls to handle slow terminals returning the two
             * chars at different times. */
            if (read(linenoise_ctx->in.fd, seq, 1) == -1)
            {
                break;
            }
            if (read(linenoise_ctx->in.fd, seq + 1, 1) == -1)
            {
                break;
            }

            /* ESC [ sequences. */
            if (seq[0] == '[')
            {
                if (seq[1] >= '0' && seq[1] <= '9')
                {
                    /* Extended escape, read additional byte. */
                    if (read(linenoise_ctx->in.fd, seq + 2, 1) == -1)
                    {
                        break;
                    }
                    if (seq[2] == '~')
                    {
                        switch (seq[1])
                        {
                        case '3': /* Delete key. */
                            linenoise_edit_delete(linenoise_ctx, l);
                            break;
                        }
                    }
                }
                else
                {
                    switch (seq[1])
                    {
                    case 'A': /* Up */
                        linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_PREV);
                        break;

                    case 'B': /* Down */
                        linenoise_edit_history_next(linenoise_ctx, l, LINENOISE_HISTORY_NEXT);
                        break;

                    case 'C': /* Right */
                        linenoise_edit_move_right(linenoise_ctx, l);
                        break;

                    case 'D': /* Left */
                        linenoise_edit_move_left(linenoise_ctx, l);
                        break;

                    case 'H': /* Home */
                        linenoise_edit_move_home(linenoise_ctx, l);
                        break;

                    case 'F': /* End*/
                        linenoise_edit_move_end(linenoise_ctx, l);
                        break;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O')
            {
                switch (seq[1])
                {
                case 'H': /* Home */
                    linenoise_edit_move_home(linenoise_ctx, l);
                    break;

                case 'F': /* End*/
                    linenoise_edit_move_end(linenoise_ctx, l);
                    break;
                }
            }
            break;

        default:
            if (linenoise_edit_insert(linenoise_ctx, l, c) != 0)
            {
                return -1;
            }
            break;

        case CTRL_U: /* Ctrl+u, delete the whole line. */
            delete_whole_line(linenoise_ctx, l);
            break;

        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            l->line_buf->b[l->pos] = '\0';
            l->len = l->pos;
            refresh_line(linenoise_ctx, l);
            break;

        case CTRL_A: /* Ctrl+a, go to the start of the line */
            linenoise_edit_move_home(linenoise_ctx, l);
            break;

        case CTRL_E: /* ctrl+e, go to the end of the line */
            linenoise_edit_move_end(linenoise_ctx, l);
            break;

        case CTRL_L: /* ctrl+l, clear screen */
            linenoise_clear_screen(linenoise_ctx);
            refresh_line(linenoise_ctx, l);
            break;

        case CTRL_W: /* ctrl+w, delete previous word */
            linenoise_edit_delete_prev_word(linenoise_ctx, l);
            break;
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
    count = linenoise_edit(linenoise_ctx, linenoise_ctx->in.fd, linenoise_ctx->out.fd, line_buf, prompt);
    disable_raw_mode(linenoise_ctx, linenoise_ctx->in.fd);
    fprintf(linenoise_ctx->out.stream, "\n");

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
    struct linenoise_st * const linenoise_ctx = calloc(1, sizeof *linenoise_ctx);

    if (linenoise_ctx == NULL)
    {
        goto done;
    }

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

