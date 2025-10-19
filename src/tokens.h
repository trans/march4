/*
 * March Language - Token Stream Reader
 * Simple whitespace-delimited token reading
 */

#ifndef MARCH_TOKENS_H
#define MARCH_TOKENS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Token types */
typedef enum {
    TOK_EOF,
    TOK_NUMBER,      /* Integer literal */
    TOK_WORD,        /* Word/identifier */
    TOK_COLON,       /* : (start definition) */
    TOK_SEMICOLON,   /* ; (end definition) */
    TOK_LPAREN,      /* ( (start quotation) */
    TOK_RPAREN,      /* ) (end quotation) */
    TOK_COMMENT,     /* -- comment (skip) */
} token_type_t;

/* Token structure */
typedef struct {
    token_type_t type;
    char* text;          /* Token text (owned, must free) */
    int64_t number;      /* For TOK_NUMBER */
    int line;            /* Line number for errors */
    int column;          /* Column for errors */
} token_t;

/* Token stream */
typedef struct {
    FILE* file;
    char* filename;
    int line;
    int column;
    char* buffer;        /* Read buffer */
    size_t buffer_size;
    size_t buffer_pos;
    bool eof;
} token_stream_t;

/* Create/destroy token stream */
token_stream_t* token_stream_create(const char* filename);
void token_stream_free(token_stream_t* stream);

/* Read next token */
bool token_stream_next(token_stream_t* stream, token_t* token);

/* Free token data */
void token_free(token_t* token);

/* Check if token matches */
bool token_is(token_t* token, const char* text);

#endif /* MARCH_TOKENS_H */
