/*
 * March Language - Dictionary (Word Lookup)
 * Hash table for word definitions with type signatures
 */

#ifndef MARCH_DICTIONARY_H
#define MARCH_DICTIONARY_H

#include <stddef.h>
#include "types.h"

/* Type signature structure */
typedef struct {
    type_id_t inputs[8];   /* Input types */
    int input_count;
    type_id_t outputs[8];  /* Output types */
    int output_count;
} type_sig_t;

/* Forward declaration for immediate handler */
typedef struct compiler compiler_t;
typedef bool (*immediate_handler_t)(compiler_t* comp);

/* Dictionary entry (word definition) */
typedef struct dict_entry {
    char* name;              /* Word name */
    void* addr;              /* Address (for primitives) or NULL */
    char* cid;               /* Content ID (for user words or primitives) */
    uint16_t prim_id;        /* Primitive ID (LINKING.md design, 0 if not primitive) */
    type_sig_t signature;    /* Type signature */
    bool is_primitive;       /* true = asm primitive, false = user word */
    bool is_immediate;       /* true = compile-time word (like if, true, false) */
    immediate_handler_t handler; /* Handler for immediate words or NULL */
    int priority;            /* For overload resolution (higher = more specific) */
    struct dict_entry* next; /* Hash table chain */
} dict_entry_t;

/* Dictionary structure */
typedef struct {
    dict_entry_t** buckets;
    size_t bucket_count;
    size_t entry_count;
} dictionary_t;

/* Create/destroy dictionary */
dictionary_t* dict_create(void);
void dict_free(dictionary_t* dict);

/* Add word to dictionary */
bool dict_add(dictionary_t* dict, const char* name, void* addr,
              const char* cid, uint16_t prim_id, type_sig_t* sig, bool is_primitive,
              bool is_immediate, immediate_handler_t handler);

/* Lookup word by name (returns first match) */
dict_entry_t* dict_lookup(dictionary_t* dict, const char* name);

/* Lookup word by name + type signature (for overload resolution) */
dict_entry_t* dict_lookup_typed(dictionary_t* dict, const char* name,
                                type_id_t* type_stack, int stack_depth);

/* Parse type signature string (e.g., "i64 i64 -> i64") */
bool parse_type_sig(const char* str, type_sig_t* sig);

/* Print type signature (for debugging) */
void print_type_sig(type_sig_t* sig);

#endif /* MARCH_DICTIONARY_H */
