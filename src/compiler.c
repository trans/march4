/*
 * March Language - One-Pass Compiler Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "compiler.h"
#include "primitives.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Create compiler */
compiler_t* compiler_create(dictionary_t* dict, march_db_t* db) {
    compiler_t* comp = malloc(sizeof(compiler_t));
    if (!comp) return NULL;

    comp->dict = dict;
    comp->db = db;
    comp->type_stack_depth = 0;
    comp->cells = cell_buffer_create();
    comp->verbose = false;

    if (!comp->cells) {
        free(comp);
        return NULL;
    }

    return comp;
}

/* Free compiler */
void compiler_free(compiler_t* comp) {
    if (comp) {
        cell_buffer_free(comp->cells);
        free(comp);
    }
}

/* Register primitives */
void compiler_register_primitives(compiler_t* comp) {
    register_primitives(comp->dict);
}

/* Push type onto type stack */
static void push_type(compiler_t* comp, type_id_t type) {
    if (comp->type_stack_depth >= MAX_TYPE_STACK) {
        fprintf(stderr, "Type stack overflow\n");
        return;
    }
    comp->type_stack[comp->type_stack_depth++] = type;
}

/* Pop type from type stack */
static type_id_t pop_type(compiler_t* comp) {
    if (comp->type_stack_depth == 0) {
        fprintf(stderr, "Type stack underflow\n");
        return TYPE_ANY;
    }
    return comp->type_stack[--comp->type_stack_depth];
}

/* Apply word's type signature to type stack */
static bool apply_signature(compiler_t* comp, type_sig_t* sig) {
    /* Check we have enough inputs */
    if (comp->type_stack_depth < sig->input_count) {
        fprintf(stderr, "Type error: need %d inputs, have %d\n",
                sig->input_count, comp->type_stack_depth);
        return false;
    }

    /* Pop inputs and verify types */
    for (int i = sig->input_count - 1; i >= 0; i--) {
        type_id_t expected = sig->inputs[i];
        type_id_t actual = pop_type(comp);

        /* TYPE_ANY matches anything */
        if (expected != TYPE_ANY && actual != TYPE_ANY && expected != actual) {
            fprintf(stderr, "Type mismatch: expected %d, got %d\n", expected, actual);
            return false;
        }
    }

    /* Push outputs */
    for (int i = 0; i < sig->output_count; i++) {
        push_type(comp, sig->outputs[i]);
    }

    return true;
}

/* Compile a number literal */
static bool compile_number(compiler_t* comp, int64_t num) {
    /* Emit LIT cell */
    cell_buffer_append(comp->cells, encode_lit(num));

    /* Push i64 type */
    push_type(comp, TYPE_I64);

    if (comp->verbose) {
        printf("  LIT %ld → i64\n", num);
    }

    return true;
}

/* Compile a word reference */
static bool compile_word(compiler_t* comp, const char* name) {
    /* Lookup word with type checking */
    dict_entry_t* entry = dict_lookup_typed(comp->dict, name,
                                            comp->type_stack,
                                            comp->type_stack_depth);

    if (!entry) {
        fprintf(stderr, "Unknown word: %s\n", name);
        return false;
    }

    /* Apply type signature */
    if (!apply_signature(comp, &entry->signature)) {
        fprintf(stderr, "Type error in word: %s\n", name);
        return false;
    }

    /* Emit XT cell */
    if (entry->is_primitive) {
        /* For primitives, emit XT with address */
        cell_buffer_append(comp->cells, encode_xt(entry->addr));
    } else {
        /* For user words, we'll need to resolve CID to address later (linking) */
        /* For now, just emit a placeholder - loader will patch this */
        cell_buffer_append(comp->cells, encode_xt(NULL));
        fprintf(stderr, "Warning: user word linking not yet implemented: %s\n", name);
    }

    if (comp->verbose) {
        printf("  XT %s", name);
        print_type_sig(&entry->signature);
        printf("\n");
    }

    return true;
}

/* Compile a word definition */
static bool compile_definition(compiler_t* comp, token_stream_t* stream) {
    /* Read word name */
    token_t name_tok;
    if (!token_stream_next(stream, &name_tok)) {
        fprintf(stderr, "Expected word name after ':'\n");
        return false;
    }

    if (name_tok.type != TOK_WORD) {
        fprintf(stderr, "Expected word name, got token type %d\n", name_tok.type);
        token_free(&name_tok);
        return false;
    }

    char* word_name = strdup(name_tok.text);
    token_free(&name_tok);

    if (comp->verbose) {
        printf("\nCompiling word: %s\n", word_name);
    }

    /* Reset cell buffer and type stack for new definition */
    cell_buffer_clear(comp->cells);
    comp->type_stack_depth = 0;

    /* Build source text as we compile */
    char* source_text = malloc(4096);  /* Initial buffer */
    if (!source_text) {
        free(word_name);
        return false;
    }
    source_text[0] = '\0';
    size_t source_len = 0;
    size_t source_cap = 4096;

    /* Compile tokens until ';' */
    token_t tok;
    while (token_stream_next(stream, &tok)) {
        if (tok.type == TOK_SEMICOLON) {
            token_free(&tok);
            break;
        }

        /* Append token text to source (with space separator) */
        size_t tok_len = strlen(tok.text);
        size_t needed = source_len + tok_len + 2;  /* +2 for space and null */
        if (needed > source_cap) {
            source_cap = needed * 2;
            char* new_buf = realloc(source_text, source_cap);
            if (!new_buf) {
                free(source_text);
                free(word_name);
                token_free(&tok);
                return false;
            }
            source_text = new_buf;
        }
        if (source_len > 0) {
            source_text[source_len++] = ' ';
        }
        strcpy(source_text + source_len, tok.text);
        source_len += tok_len;

        bool success = false;

        if (tok.type == TOK_NUMBER) {
            success = compile_number(comp, tok.number);
        } else if (tok.type == TOK_WORD) {
            success = compile_word(comp, tok.text);
        } else {
            fprintf(stderr, "Unexpected token in definition: %s\n", tok.text);
        }

        token_free(&tok);

        if (!success) {
            free(source_text);
            free(word_name);
            return false;
        }
    }

    /* Emit EXIT */
    cell_buffer_append(comp->cells, encode_exit());

    /* Build type signature from final stack state */
    /* For now, just use "-> " + output types */
    /* TODO: input type inference for words without literals */
    char type_sig[256];
    if (comp->type_stack_depth > 0) {
        strcpy(type_sig, "-> ");
        for (int i = 0; i < comp->type_stack_depth; i++) {
            switch (comp->type_stack[i]) {
                case TYPE_I64: strcat(type_sig, "i64 "); break;
                case TYPE_U64: strcat(type_sig, "u64 "); break;
                case TYPE_F64: strcat(type_sig, "f64 "); break;
                case TYPE_PTR: strcat(type_sig, "ptr "); break;
                case TYPE_BOOL: strcat(type_sig, "bool "); break;
                case TYPE_STR: strcat(type_sig, "str "); break;
                case TYPE_ANY: strcat(type_sig, "any "); break;
                case TYPE_UNKNOWN: strcat(type_sig, "? "); break;
            }
        }
    } else {
        strcpy(type_sig, "->");
    }

    if (comp->verbose) {
        printf("  Type signature: %s\n", type_sig);
        printf("  Source: %s\n", source_text);
        printf("  %zu cells, %zu bytes\n",
               comp->cells->count, comp->cells->count * 8);
    }

    /* Store in database with source text */
    bool stored = db_store_word(comp->db, word_name, "user",
                                (uint8_t*)comp->cells->cells,
                                comp->cells->count,
                                type_sig,
                                source_text);

    if (!stored) {
        fprintf(stderr, "Failed to store word: %s\n", word_name);
        free(source_text);
        free(word_name);
        return false;
    }

    if (comp->verbose) {
        printf("  ✓ Stored word: %s\n", word_name);
    }

    /* Add to dictionary so it can be used in later definitions */
    type_sig_t sig;
    if (parse_type_sig(type_sig, &sig)) {
        dict_add(comp->dict, word_name, NULL, NULL, &sig, false);
    }

    free(source_text);
    free(word_name);
    return true;
}

/* Compile a file */
bool compiler_compile_file(compiler_t* comp, const char* filename) {
    token_stream_t* stream = token_stream_create(filename);
    if (!stream) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return false;
    }

    if (comp->verbose) {
        printf("Compiling: %s\n", filename);
    }

    token_t tok;
    while (token_stream_next(stream, &tok)) {
        bool success = true;

        if (tok.type == TOK_COLON) {
            /* Start word definition */
            success = compile_definition(comp, stream);
        } else if (tok.type == TOK_NUMBER || tok.type == TOK_WORD) {
            /* Top-level expressions not yet supported */
            fprintf(stderr, "Top-level expressions not supported yet: %s\n", tok.text);
            success = false;
        }

        token_free(&tok);

        if (!success) {
            token_stream_free(stream);
            return false;
        }
    }

    token_stream_free(stream);
    return true;
}
