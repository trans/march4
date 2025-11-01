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
/* Read string literal: "..." with escape sequences */
static bool read_string(token_stream_t* stream, token_t* token) {
    char buffer[1024];  /* Larger buffer for strings */
    int pos = 0;
    int c;

    token->line = stream->line;
    token->column = stream->column;

    /* Skip opening " */
    c = fgetc(stream->file);
    if (c != '"') {
        fprintf(stderr, "Internal error: read_string called without opening quote\n");
        return false;
    }
    stream->column++;

    /* Read until closing " followed by whitespace */
    while ((c = fgetc(stream->file)) != EOF) {
        stream->column++;

        if (c == '\\') {
            /* Escape sequence */
            int next = fgetc(stream->file);
            if (next == EOF) {
                fprintf(stderr, "Line %d: Unexpected EOF after backslash in string\n", stream->line);
                return false;
            }
            stream->column++;

            if (next == '"') {
                /* Escaped quote */
                if (pos < 1023) buffer[pos++] = '"';
            } else if (next == '\\') {
                /* Escaped backslash */
                if (pos < 1023) buffer[pos++] = '\\';
            } else {
                /* Unknown escape - keep both characters */
                if (pos < 1023) buffer[pos++] = '\\';
                if (pos < 1023) buffer[pos++] = next;
            }
        } else if (c == '"') {
            /* Potential closing quote - check for whitespace after */
            int next = fgetc(stream->file);
            if (next == EOF || isspace(next)) {
                /* Valid string end */
                if (next != EOF) {
                    ungetc(next, stream->file);
                }
                buffer[pos] = '\0';
                token->type = TOK_STRING;
                token->text = strdup(buffer);
                return true;
            } else {
                /* Quote not followed by whitespace - include it */
                if (pos < 1023) buffer[pos++] = '"';
                ungetc(next, stream->file);
            }
        } else {
            /* Regular character */
            if (pos < 1023) buffer[pos++] = c;
        }
    }

    fprintf(stderr, "Line %d: Unterminated string literal\n", stream->line);
    return false;
}

static bool read_word(token_stream_t* stream, token_t* token) {
    char buffer[256];
    int pos = 0;
    int c;

    token->line = stream->line;
    token->column = stream->column;

    /* Check for string literal */
    c = fgetc(stream->file);
    if (c == '"') {
        ungetc(c, stream->file);
        return read_string(stream, token);
    }
    ungetc(c, stream->file);

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
    } else if (strcmp(buffer, "(") == 0) {
        token->type = TOK_LPAREN;
    } else if (strcmp(buffer, ")") == 0) {
        token->type = TOK_RPAREN;
    } else if (strcmp(buffer, "[") == 0) {
        token->type = TOK_LBRACKET;
    } else if (strcmp(buffer, "]") == 0) {
        token->type = TOK_RBRACKET;
    } else if (strcmp(buffer, "$") == 0) {
        token->type = TOK_DOLLAR;
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
