/* linenoise.h -- VERSION 1.0
 *
 * Guerrilla line editing library against the idea that a line editing lib
 * needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>
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
 */

#ifndef LINENOISE_H__
#define LINENOISE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct linenoise_st linenoise_st;

typedef struct linenoise_completions linenoise_completions;

typedef void(linenoise_completion_callback)(const char *, linenoise_completions *);

void
linenoise_set_completion_callback(linenoise_st * linenoise_ctx, linenoise_completion_callback * cb);

void
linenoise_add_completion(linenoise_completions * completions, char const * completion);

/*
 * Get the current pointer to the line buffer. Note that any changes made by
 * callbacks may result in this pointer becoming invalid, so it should be
 * reobtained after any modification.
 */
char *
linenoise_line_get(linenoise_st * linenoise_ctx);

size_t
linenoise_point_get(linenoise_st * linenoise_ctx);

size_t
linenoise_end_get(linenoise_st * linenoise_ctx);

void
linenoise_point_set(
    linenoise_st * linenoise_ctx,
    unsigned new_point);

void
linenoise_delete_text(
    linenoise_st * linenoise_ctx,
    unsigned start,
    unsigned end);

bool
linenoise_insert_text_len(
    linenoise_st * linenoise_ctx,
    char const * text,
    unsigned delta);

bool
linenoise_insert_text(linenoise_st * linenoise_ctx, char const * text);

int
linenoise_terminal_width(linenoise_st * linenoise_ctx);

bool linenoise_complete(
    linenoise_st * linenoise_ctx,
    unsigned start,
    char * * matches,
    bool allow_prefix);

void
linenoise_display_matches(
    linenoise_st * linenoise_ctx,
    char * * matches);

bool
linenoise_refresh_line(linenoise_st * linenoise_ctx);

typedef enum linenoise_key_handler_flags_t
{
	linenoise_key_handler_done = 0x01,
    linenoise_key_handler_refresh = 0x02,
    linenoise_key_handler_error = 0x04
} linenoise_key_handler_flags_t;

typedef bool (*linenoise_key_binding_handler_cb)(
	linenoise_st *linenoise_ctx,
	uint32_t * flags,
    char const * key,
    void * user_ctx);

void
linenoise_bind_key(
    linenoise_st * linenoise_ctx,
    uint8_t key,
    linenoise_key_binding_handler_cb handler,
    void * user_ctx);

void
linenoise_bind_keyseq(
    linenoise_st * linenoise_ctx,
    const char * seq,
    linenoise_key_binding_handler_cb handler,
    void * context);

char *
linenoise(linenoise_st * linenoise_ctx, char const * prompt);

void
linenoise_free(void *ptr);

int
linenoise_history_add(linenoise_st * linenoise_ctx, char const * line);

int
linenoise_history_set_max_len(linenoise_st * linenoise_ctx, int len);

void
linenoise_clear_screen(linenoise_st * linenoise_ctx);

void
linenoise_set_mask_mode(linenoise_st * linenoise_ctx, bool enable);

struct linenoise_st *
linenoise_new(FILE * in_stream, FILE * out_stream);

void
linenoise_delete(linenoise_st * linenoise);

int
linenoise_printf(
    linenoise_st * const linenoise_ctx,
    char const * const fmt,
    ...);

#ifdef __cplusplus
}
#endif

#endif /* LINENOISE_H__ */

