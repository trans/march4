/*
 * March Language - One-Pass Compiler
 */

#ifndef MARCH_COMPILER_H
#define MARCH_COMPILER_H

#include "types.h"
#include "cells.h"
#include "tokens.h"
#include "dictionary.h"
#include "database.h"

/* Maximum quotation nesting depth */
#define MAX_QUOT_DEPTH 16

/* Maximum pending quotation references in a single word */
#define MAX_QUOT_REFS 64

/* Quotation kind */
typedef enum {
    QUOT_LITERAL,  /* Lexical - uncompiled tokens, compile at use site */
    QUOT_TYPED     /* Typed - compiled with concrete types */
} quot_kind_t;

/* Quotation (compile-time only) */
typedef struct {
    quot_kind_t kind;              /* Literal vs Typed */

    /* For QUOT_TYPED: compiled form */
    cell_buffer_t* cells;          /* Legacy: runtime cells */
    blob_buffer_t* blob;           /* CID-based: blob encoding */

    /* For QUOT_LITERAL: token storage */
    token_t* tokens;               /* Array of captured tokens */
    int token_count;
    int token_capacity;

    /* Type information */
    type_id_t inputs[MAX_TYPE_STACK];
    int input_count;
    type_id_t outputs[MAX_TYPE_STACK];
    int output_count;
} quotation_t;

/* Compiler state */
typedef struct compiler {
    dictionary_t* dict;
    march_db_t* db;
    type_id_t type_stack[MAX_TYPE_STACK];
    int type_stack_depth;
    cell_buffer_t* cells;          /* Legacy: runtime cells (deprecated) */
    blob_buffer_t* blob;           /* CID-based blob encoding (LINKING.md) */
    bool verbose;

    /* Type signature for next word definition (from $ declaration) */
    type_sig_t* pending_type_sig;

    /* Quotation compilation support */
    quotation_t* quot_stack[MAX_QUOT_DEPTH];
    int quot_stack_depth;

    /* Buffer stack for nested quotations */
    cell_buffer_t* buffer_stack[MAX_QUOT_DEPTH + 1];  /* +1 for root */
    int buffer_stack_depth;

    /* Blob buffer stack for nested quotations (CID-based) */
    blob_buffer_t* blob_stack[MAX_QUOT_DEPTH + 1];    /* +1 for root */
    int blob_stack_depth;

    /* Runtime quotation counter for generating unique names (deprecated) */
    int quot_counter;

    /* Pending quotation CID references (for linking) */
    unsigned char* pending_quot_cids[MAX_QUOT_REFS];
    int pending_quot_count;
} compiler_t;

/* Create/free compiler */
compiler_t* compiler_create(dictionary_t* dict, march_db_t* db);
void compiler_free(compiler_t* comp);

/* Compile a file */
bool compiler_compile_file(compiler_t* comp, const char* filename);

/* Register primitives */
void compiler_register_primitives(compiler_t* comp);

#endif /* MARCH_COMPILER_H */
