/*
 * March Language - Word Loader Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Create loader */
loader_t* loader_create(march_db_t* db, dictionary_t* dict) {
    loader_t* loader = malloc(sizeof(loader_t));
    if (!loader) return NULL;

    loader->db = db;
    loader->dict = dict;
    loader->word_capacity = 64;
    loader->word_count = 0;
    loader->words = calloc(loader->word_capacity, sizeof(loaded_word_t*));

    if (!loader->words) {
        free(loader);
        return NULL;
    }

    return loader;
}

/* Free loaded word */
static void loaded_word_free(loaded_word_t* word) {
    if (word) {
        free(word->name);
        free(word->cells);
        free(word);
    }
}

/* Free loader */
void loader_free(loader_t* loader) {
    if (loader) {
        for (size_t i = 0; i < loader->word_count; i++) {
            loaded_word_free(loader->words[i]);
        }
        free(loader->words);
        free(loader);
    }
}

/* Find loaded word by name */
loaded_word_t* loader_find_word(loader_t* loader, const char* name) {
    for (size_t i = 0; i < loader->word_count; i++) {
        if (strcmp(loader->words[i]->name, name) == 0) {
            return loader->words[i];
        }
    }
    return NULL;
}

/* Load a word from database into memory */
loaded_word_t* loader_load_word(loader_t* loader, const char* name, const char* namespace) {
    /* Check if already loaded */
    loaded_word_t* existing = loader_find_word(loader, name);
    if (existing) {
        return existing;
    }

    /* Load from database */
    size_t cell_count = 0;
    uint64_t* cells = db_load_word(loader->db, name, namespace, &cell_count);
    if (!cells) {
        fprintf(stderr, "Failed to load word: %s\n", name);
        return NULL;
    }

    /* Create loaded word structure */
    loaded_word_t* word = malloc(sizeof(loaded_word_t));
    if (!word) {
        free(cells);
        return NULL;
    }

    word->name = strdup(name);
    word->cells = cells;
    word->cell_count = cell_count;
    word->entry_point = (void*)cells;  /* Entry point is address of first cell */

    /* Add to loaded words cache */
    if (loader->word_count >= loader->word_capacity) {
        loader->word_capacity *= 2;
        loader->words = realloc(loader->words, loader->word_capacity * sizeof(loaded_word_t*));
    }
    loader->words[loader->word_count++] = word;

    return word;
}

/* Get entry point address for a word */
void* loader_get_entry_point(loader_t* loader, const char* name) {
    loaded_word_t* word = loader_find_word(loader, name);
    if (word) {
        return word->entry_point;
    }

    /* Try loading it */
    word = loader_load_word(loader, name, "user");
    if (word) {
        return word->entry_point;
    }

    return NULL;
}

/* Link/resolve XT references in loaded words */
bool loader_link(loader_t* loader) {
    /* TODO: Implement word linking
     *
     * Current limitation: When compiling `: fifteen five ten + ;`,
     * we emit XT(NULL) for user words because we don't know their addresses yet.
     *
     * To implement linking, we need to:
     * 1. Store word reference metadata (which XT cell refers to which word name)
     * 2. Scan loaded words for XT(NULL) cells
     * 3. Look up referenced words (loading them if needed)
     * 4. Patch XT cells with actual addresses
     *
     * Possible approaches:
     * - Store relocation info in database (edges table or defs metadata)
     * - Use LST cells to encode word name references
     * - Store separate relocation table alongside compiled code
     *
     * For now, words that only use primitives and literals will work.
     */

    fprintf(stderr, "Warning: Word linking not yet implemented\n");
    fprintf(stderr, "  Words calling other user words will not work yet\n");
    fprintf(stderr, "  Words with only primitives and literals are OK\n");

    return true;
}
