/*
 * March Language - Dictionary Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "dictionary.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_BUCKETS 256

/* Simple hash function (djb2) */
static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

dictionary_t* dict_create(void) {
    dictionary_t* dict = malloc(sizeof(dictionary_t));
    if (!dict) return NULL;

    dict->bucket_count = INITIAL_BUCKETS;
    dict->entry_count = 0;
    dict->buckets = calloc(INITIAL_BUCKETS, sizeof(dict_entry_t*));

    if (!dict->buckets) {
        free(dict);
        return NULL;
    }

    return dict;
}

void dict_free(dictionary_t* dict) {
    if (!dict) return;

    for (size_t i = 0; i < dict->bucket_count; i++) {
        dict_entry_t* entry = dict->buckets[i];
        while (entry) {
            dict_entry_t* next = entry->next;
            free(entry->name);
            free(entry->cid);
            free(entry);
            entry = next;
        }
    }

    free(dict->buckets);
    free(dict);
}

bool dict_add(dictionary_t* dict, const char* name, void* addr,
              const char* cid, uint16_t prim_id, type_sig_t* sig, bool is_primitive,
              bool is_immediate, immediate_handler_t handler) {
    unsigned long hash = hash_string(name);
    size_t bucket = hash % dict->bucket_count;

    /* Create new entry */
    dict_entry_t* entry = malloc(sizeof(dict_entry_t));
    if (!entry) return false;

    entry->name = strdup(name);
    entry->addr = addr;
    entry->cid = cid ? strdup(cid) : NULL;
    entry->prim_id = prim_id;
    entry->is_primitive = is_primitive;
    entry->is_immediate = is_immediate;
    entry->handler = handler;

    if (sig) {
        memcpy(&entry->signature, sig, sizeof(type_sig_t));
        /* Calculate priority for overload resolution */
        entry->priority = 0;
        for (int i = 0; i < sig->input_count; i++) {
            if (sig->inputs[i] == TYPE_I64 || sig->inputs[i] == TYPE_U64) {
                entry->priority += 100;  /* Concrete types have higher priority */
            } else if (sig->inputs[i] == TYPE_ANY) {
                entry->priority += 10;   /* Polymorphic types are lower priority */
            }
        }
    } else {
        memset(&entry->signature, 0, sizeof(type_sig_t));
        entry->priority = 0;
    }

    /* Insert at head of chain */
    entry->next = dict->buckets[bucket];
    dict->buckets[bucket] = entry;
    dict->entry_count++;

    return true;
}

dict_entry_t* dict_lookup(dictionary_t* dict, const char* name) {
    unsigned long hash = hash_string(name);
    size_t bucket = hash % dict->bucket_count;

    dict_entry_t* entry = dict->buckets[bucket];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/* Calculate match score for overload resolution */
static int match_score(type_sig_t* sig, type_id_t* type_stack, int stack_depth) {
    /* Check if stack has enough items */
    if (stack_depth < sig->input_count) {
        return -1;  /* Not enough items on stack */
    }

    int score = 0;

    /* Check input types (stack grows down, so top is at stack_depth-1) */
    for (int i = 0; i < sig->input_count; i++) {
        int stack_pos = stack_depth - sig->input_count + i;
        type_id_t stack_type = type_stack[stack_pos];
        type_id_t sig_type = sig->inputs[i];

        if (sig_type == TYPE_ANY) {
            score += 10;  /* Polymorphic type matches anything */
        } else if (stack_type == sig_type) {
            score += 100; /* Exact match */
        } else if (stack_type == TYPE_UNKNOWN) {
            score += 50;  /* Unknown type (could match) */
        } else {
            return -1;    /* Type mismatch */
        }
    }

    return score;
}

dict_entry_t* dict_lookup_typed(dictionary_t* dict, const char* name,
                                type_id_t* type_stack, int stack_depth) {
    unsigned long hash = hash_string(name);
    size_t bucket = hash % dict->bucket_count;

    dict_entry_t* best = NULL;
    int best_score = -1;

    /* Find all candidates and pick the best match */
    dict_entry_t* entry = dict->buckets[bucket];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            int score = match_score(&entry->signature, type_stack, stack_depth);
            if (score > best_score) {
                best = entry;
                best_score = score;
            } else if (score == best_score && best) {
                /* Tie-break by priority */
                if (entry->priority > best->priority) {
                    best = entry;
                }
            }
        }
        entry = entry->next;
    }

    return best;
}

/* Parse type string (e.g., "i64", "u64", "any") */
static type_id_t parse_type(const char* str) {
    if (strcmp(str, "i64") == 0) return TYPE_I64;
    if (strcmp(str, "u64") == 0) return TYPE_U64;
    if (strcmp(str, "f64") == 0) return TYPE_F64;
    if (strcmp(str, "ptr") == 0) return TYPE_PTR;
    if (strcmp(str, "bool") == 0) return TYPE_BOOL;
    if (strcmp(str, "str") == 0) return TYPE_STR;
    if (strcmp(str, "any") == 0) return TYPE_ANY;
    return TYPE_UNKNOWN;
}

bool parse_type_sig(const char* str, type_sig_t* sig) {
    memset(sig, 0, sizeof(type_sig_t));

    char buffer[MAX_TYPE_SIG];
    strncpy(buffer, str, MAX_TYPE_SIG - 1);
    buffer[MAX_TYPE_SIG - 1] = '\0';

    char* ptr = buffer;
    bool reading_outputs = false;

    /* Tokenize by whitespace */
    char* token = strtok(ptr, " \t");
    while (token) {
        if (strcmp(token, "->") == 0 || strcmp(token, "→") == 0) {
            reading_outputs = true;
        } else {
            type_id_t type = parse_type(token);
            if (type == TYPE_UNKNOWN) {
                return false;  /* Invalid type */
            }

            if (reading_outputs) {
                if (sig->output_count < 8) {
                    sig->outputs[sig->output_count++] = type;
                }
            } else {
                if (sig->input_count < 8) {
                    sig->inputs[sig->input_count++] = type;
                }
            }
        }
        token = strtok(NULL, " \t");
    }

    return true;
}

/* Type to string */
static const char* type_to_string(type_id_t type) {
    switch (type) {
        case TYPE_I64: return "i64";
        case TYPE_U64: return "u64";
        case TYPE_F64: return "f64";
        case TYPE_PTR: return "ptr";
        case TYPE_BOOL: return "bool";
        case TYPE_STR: return "str";
        case TYPE_ANY: return "any";
        default: return "???";
    }
}

void print_type_sig(type_sig_t* sig) {
    for (int i = 0; i < sig->input_count; i++) {
        printf("%s ", type_to_string(sig->inputs[i]));
    }
    printf("-> ");
    for (int i = 0; i < sig->output_count; i++) {
        printf("%s ", type_to_string(sig->outputs[i]));
    }
}
