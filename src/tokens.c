/*
 * March Language - Token Stream Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "tokens.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define BUFFER_SIZE 4096

token_stream_t* token_stream_create(const char* filename) {
    token_stream_t* stream = malloc(sizeof(token_stream_t));
    if (!stream) return NULL;

    stream->file = fopen(filename, "r");
    if (!stream->file) {
        free(stream);
        return NULL;
    }

    stream->filename = strdup(filename);
    stream->line = 1;
    stream->column = 1;
    stream->eof = false;

    stream->buffer = malloc(BUFFER_SIZE);
    stream->buffer_size = BUFFER_SIZE;
    stream->buffer_pos = 0;

    return stream;
}

void token_stream_free(token_stream_t* stream) {
    if (stream) {
        if (stream->file) fclose(stream->file);
        free(stream->filename);
        free(stream->buffer);
        free(stream);
    }
}

void token_free(token_t* token) {
    if (token && token->text) {
        free(token->text);
        token->text = NULL;
    }
}

/* Read next character, tracking position */
static int next_char(token_stream_t* stream) {
    int c = fgetc(stream->file);

    if (c == '\n') {
        stream->line++;
        stream->column = 1;
    } else if (c != EOF) {
        stream->column++;
    } else {
        stream->eof = true;
    }

    return c;
}

/* Skip whitespace */
static void skip_whitespace(token_stream_t* stream) {
    int c;
    while ((c = fgetc(stream->file)) != EOF && isspace(c)) {
        if (c == '\n') {
            stream->line++;
            stream->column = 1;
        } else {
            stream->column++;
        }
    }

    if (c != EOF) {
        ungetc(c, stream->file);
    } else {
        stream->eof = true;
    }
}

/* Read a word token */
static bool read_word(token_stream_t* stream, token_t* token) {
    char buffer[256];
    int pos = 0;
    int c;

    token->line = stream->line;
    token->column = stream->column;

    while ((c = fgetc(stream->file)) != EOF && !isspace(c)) {
        if (pos < 255) {
            buffer[pos++] = c;
        }
        stream->column++;
    }

    if (c != EOF) {
        ungetc(c, stream->file);
    }

    buffer[pos] = '\0';

    /* Check for special tokens */
    if (strcmp(buffer, ":") == 0) {
        token->type = TOK_COLON;
    } else if (strcmp(buffer, ";") == 0) {
        token->type = TOK_SEMICOLON;
    } else if (strcmp(buffer, "--") == 0) {
        /* Comment - skip to end of line */
        while ((c = fgetc(stream->file)) != EOF && c != '\n') {
            stream->column++;
        }
        if (c == '\n') {
            stream->line++;
            stream->column = 1;
        }
        token->type = TOK_COMMENT;
    } else {
        /* Try to parse as number */
        char* endptr;
        errno = 0;
        int64_t num = strtoll(buffer, &endptr, 0);  /* Base 0 = auto-detect (dec/hex/oct) */

        if (*endptr == '\0' && errno == 0) {
            /* Valid number */
            token->type = TOK_NUMBER;
            token->number = num;
        } else {
            /* Word/identifier */
            token->type = TOK_WORD;
        }
    }

    token->text = strdup(buffer);
    return true;
}

bool token_stream_next(token_stream_t* stream, token_t* token) {
    if (stream->eof) {
        token->type = TOK_EOF;
        token->text = NULL;
        return false;
    }

    skip_whitespace(stream);

    if (stream->eof) {
        token->type = TOK_EOF;
        token->text = NULL;
        return false;
    }

    /* Read token */
    if (!read_word(stream, token)) {
        return false;
    }

    /* Skip comments */
    if (token->type == TOK_COMMENT) {
        token_free(token);
        return token_stream_next(stream, token);
    }

    return true;
}

bool token_is(token_t* token, const char* text) {
    return token->text && strcmp(token->text, text) == 0;
}
