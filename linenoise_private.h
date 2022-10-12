#pragma once

#include "linenoise.h"
#include "config.h"
#include "buffer.h"

#include <termios.h>

#define DEFAULT_TERMINAL_WIDTH 80
#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096

struct linenoise_completions {
  size_t len;
  char * * cvec;
};

typedef struct key_binding_st
{
    void * user_ctx;
    key_binding_handler_cb handler;
} key_binding_st;

/* The linenoiseState structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
struct linenoise_state
{
    struct buffer * line_buf;

    char const * prompt; /* Prompt to display. */
    size_t prompt_len;   /* Prompt length. */
    size_t pos;          /* Current cursor position. */
    size_t oldpos;       /* Previous refresh cursor position. */
    size_t len;          /* Current edited line length. */
    size_t cols;         /* Number of columns in terminal. */
    size_t maxrows;      /* Maximum num of rows used so far (multiline mode) */
    int history_index;   /* The history index we are currently editing. */
};

struct linenoise_st
{
    struct
    {
        FILE * stream;
        int fd;
    } in;
    struct
    {
        FILE * stream;
        int fd;
    } out;

    bool is_a_tty;
    bool in_raw_mode;
    struct termios orig_termios;

#if WITH_KEY_BINDING
    key_binding_st key_bindings[256]; /* One for each ACSII character. */
#endif

    struct linenoise_state state;

    struct
    {
        bool mask_mode;
        bool multiline_mode;
        bool disable_beep;
#if WITH_NATIVE_COMPLETION
        linenoise_completion_callback * completion_callback;
#endif
#if WITH_HINTS
        linenoise_hints_callback * hints_callback;
        linenoise_free_hints_callback * free_hints_callback;
#endif
    } options;

    struct
    {
        int max_len;
        int current_len;
        char ** history;
    } history;
};

int
linenoise_get_terminal_width(int ifd, int ofd);

bool
refresh_line(linenoise_st * linenoise_ctx, struct linenoise_state * l);

bool
refresh_line_check_row_clear(
    linenoise_st * linenoise_ctx,
    struct linenoise_state * l,
    bool row_clear_required);

int
linenoise_edit_insert(
    linenoise_st * linenoise_ctx,
    struct linenoise_state * l,
    char c);

