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

/* Compiler state */
typedef struct {
    dictionary_t* dict;
    march_db_t* db;
    type_id_t type_stack[MAX_TYPE_STACK];
    int type_stack_depth;
    cell_buffer_t* cells;
    bool verbose;
} compiler_t;

/* Create/free compiler */
compiler_t* compiler_create(dictionary_t* dict, march_db_t* db);
void compiler_free(compiler_t* comp);

/* Compile a file */
bool compiler_compile_file(compiler_t* comp, const char* filename);

/* Register primitives */
void compiler_register_primitives(compiler_t* comp);

#endif /* MARCH_COMPILER_H */
