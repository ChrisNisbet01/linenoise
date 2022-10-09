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

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct linenoise_st linenoise_st;

typedef struct linenoiseCompletions linenoiseCompletions;

typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
typedef char*(linenoiseHintsCallback)(const char *, int *color, int *bold);
typedef void(linenoiseFreeHintsCallback)(void *);
void linenoiseSetCompletionCallback(linenoise_st * linenoise_ctx, linenoiseCompletionCallback * cb);

void linenoiseSetHintsCallback(linenoise_st * linenoise_ctx, linenoiseHintsCallback * cb);
void linenoiseSetFreeHintsCallback(linenoise_st * linenoise_ctx, linenoiseFreeHintsCallback * cb);

void linenoiseAddCompletion(linenoiseCompletions * completions, const char * completion);

/*
 * Get the current pointer to the line buffer. Note that any changes made by
 * callbacks may result in this pointer becoming invalid, so it should be
 * reobtained after any modification.
 */
char * linenoise_line_get(linenoise_st * linenoise_ctx);

size_t linenoise_point_get(linenoise_st * linenoise_ctx);

void linenoise_delete_text(
    linenoise_st * const linenoise_ctx,
    unsigned start,
    unsigned end);

bool linenoise_insert_text_len(
    linenoise_st * const linenoise_ctx,
    const char * text,
    unsigned delta);

bool linenoise_insert_text(linenoise_st * const linenoise_ctx, const char * text);

bool linenoise_complete(
    linenoise_st * const linenoise_ctx,
    unsigned start,
    char * * matches,
    bool allow_prefix);


typedef bool (*key_binding_handler_cb)(linenoise_st * linenoise_ctx, char key, void * user_ctx);
void linenoise_bind_key(linenoise_st * linenoise_ctx, uint8_t key, key_binding_handler_cb handler, void * user_ctx);

char *linenoise(linenoise_st * linenoise_ctx, const char *prompt);
void linenoiseFree(void *ptr);
int linenoiseHistoryAdd(linenoise_st * linenoise_ctx, const char *line);
int linenoiseHistorySetMaxLen(linenoise_st * linenoise_ctx, int len);

#ifdef LINENOISE_HISTORY_FILE_SUPPORT
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);
#endif

void linenoiseClearScreen(linenoise_st * linenoise_ctx);
void linenoiseSetMultiLine(linenoise_st * linenoise_ctx, bool ml);

#ifdef LINENOISE_PRINT_KEY_CODES_SUPPORT
void linenoisePrintKeyCodes(linenoise_st * linenoise_ctx);
#endif
void linenoiseMaskModeEnable(linenoise_st * linenoise_ctx);
void linenoiseMaskModeDisable(linenoise_st * linenoise_ctx);

void
linenoiseBeepControl(linenoise_st * linenoise_ctx, bool enable);

struct linenoise_st *
linenoise_new(FILE * in_stream, FILE * out_stream);

void
linenoise_delete(linenoise_st * linenoise);

#ifdef __cplusplus
}
#endif

#endif /* __LINENOISE_H */
