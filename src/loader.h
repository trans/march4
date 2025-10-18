/*
 * March Language - Word Loader
 * Load compiled words from database into memory
 */

#ifndef MARCH_LOADER_H
#define MARCH_LOADER_H

#include "types.h"
#include "database.h"
#include "dictionary.h"
#include <stddef.h>
#include <stdbool.h>

/* Loaded word structure */
typedef struct {
    char* name;
    cell_t* cells;
    size_t cell_count;
    void* entry_point;  /* Address of first cell */
} loaded_word_t;

/* Loader context */
typedef struct {
    march_db_t* db;
    dictionary_t* dict;
    loaded_word_t** words;
    size_t word_count;
    size_t word_capacity;
} loader_t;

/* Create/free loader */
loader_t* loader_create(march_db_t* db, dictionary_t* dict);
void loader_free(loader_t* loader);

/* Load a word from database into memory */
loaded_word_t* loader_load_word(loader_t* loader, const char* name, const char* namespace);

/* Find loaded word by name */
loaded_word_t* loader_find_word(loader_t* loader, const char* name);

/* Link/resolve XT references in loaded words */
bool loader_link(loader_t* loader);

/* Get entry point address for a word */
void* loader_get_entry_point(loader_t* loader, const char* name);

#endif /* MARCH_LOADER_H */
