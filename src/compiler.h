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

/* Quotation (compile-time only) */
typedef struct {
    cell_buffer_t* cells;
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
    cell_buffer_t* cells;
    bool verbose;

    /* Quotation compilation support */
    quotation_t* quot_stack[MAX_QUOT_DEPTH];
    int quot_stack_depth;

    /* Buffer stack for nested quotations */
    cell_buffer_t* buffer_stack[MAX_QUOT_DEPTH + 1];  /* +1 for root */
    int buffer_stack_depth;
} compiler_t;

/* Create/free compiler */
compiler_t* compiler_create(dictionary_t* dict, march_db_t* db);
void compiler_free(compiler_t* comp);

/* Compile a file */
bool compiler_compile_file(compiler_t* comp, const char* filename);

/* Register primitives */
void compiler_register_primitives(compiler_t* comp);

#endif /* MARCH_COMPILER_H */
