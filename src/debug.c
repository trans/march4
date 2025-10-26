/*
 * March Language - Debug/Trace System Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "debug.h"
#include "types.h"
#include "dictionary.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

/* Global debug flags */
unsigned int debug_flags = 0;

/* Type names for pretty printing */
static const char* type_name(type_id_t type) {
    switch (type) {
        case TYPE_I64: return "i64";
        case TYPE_U64: return "u64";
        case TYPE_F64: return "f64";
        case TYPE_PTR: return "ptr";
        case TYPE_BOOL: return "bool";
        case TYPE_STR: return "str";
        case TYPE_ANY: return "any";
        case TYPE_UNKNOWN: return "???";
        default: return "INVALID";
    }
}

/* Initialize debug system from environment variable */
void debug_init(void) {
    const char* env = getenv("MARCH_DEBUG");
    if (!env) return;

    /* Parse comma-separated categories */
    char* str = strdup(env);
    char* token = strtok(str, ",");

    while (token) {
        /* Trim whitespace */
        while (isspace(*token)) token++;
        char* end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';

        /* Match category */
        if (strcmp(token, "all") == 0) {
            debug_flags = DEBUG_ALL;
        } else if (strcmp(token, "compiler") == 0) {
            debug_flags |= DEBUG_COMPILER;
        } else if (strcmp(token, "dict") == 0) {
            debug_flags |= DEBUG_DICT;
        } else if (strcmp(token, "types") == 0) {
            debug_flags |= DEBUG_TYPES;
        } else if (strcmp(token, "cid") == 0) {
            debug_flags |= DEBUG_CID;
        } else if (strcmp(token, "loader") == 0) {
            debug_flags |= DEBUG_LOADER;
        } else if (strcmp(token, "runtime") == 0) {
            debug_flags |= DEBUG_RUNTIME;
        } else if (strcmp(token, "db") == 0) {
            debug_flags |= DEBUG_DB;
        } else {
            fprintf(stderr, "Warning: Unknown debug category '%s'\n", token);
        }

        token = strtok(NULL, ",");
    }

    free(str);

    if (debug_flags) {
        fprintf(stderr, "[DEBUG] Enabled categories: 0x%02x\n", debug_flags);
    }
}

/* Enable debug category */
void debug_enable(debug_category_t category) {
    debug_flags |= category;
}

/* Disable debug category */
void debug_disable(debug_category_t category) {
    debug_flags &= ~category;
}

/* Check if category is enabled */
bool debug_enabled(debug_category_t category) {
    return (debug_flags & category) != 0;
}

/* Dump type stack */
void debug_dump_type_stack(const char* label, void* stack_ptr, int depth) {
    if (!debug_enabled(DEBUG_TYPES)) return;

    type_id_t* type_stack = (type_id_t*)stack_ptr;
    fprintf(stderr, "[DEBUG_TYPES] %s [", label);
    for (int i = 0; i < depth; i++) {
        fprintf(stderr, "%s%s", type_name(type_stack[i]),
                (i < depth - 1) ? ", " : "");
    }
    fprintf(stderr, "] depth=%d\n", depth);
}

/* Dump dictionary stats */
void debug_dump_dict_stats(void* dict_ptr) {
    if (!debug_enabled(DEBUG_DICT)) return;

    dictionary_t* dict = (dictionary_t*)dict_ptr;
    int total_entries = 0;
    int primitive_count = 0;
    int word_count = 0;
    int immediate_count = 0;

    for (size_t i = 0; i < dict->bucket_count; i++) {
        dict_entry_t* entry = dict->buckets[i];
        while (entry) {
            total_entries++;
            if (entry->is_primitive) primitive_count++;
            else word_count++;
            if (entry->is_immediate) immediate_count++;
            entry = entry->next;
        }
    }

    fprintf(stderr, "[DEBUG_DICT] Dictionary stats: %d total (%d primitives, %d words, %d immediate)\n",
            total_entries, primitive_count, word_count, immediate_count);
}

/* ============================================================================ */
/* Crash Handler */
/* ============================================================================ */

/* Global crash context */
crash_context_t crash_context = {
    .phase = "init",
    .current_file = NULL,
    .current_word = "",
    .current_token = "",
    .type_stack_depth = 0,
    .quot_stack_depth = 0,
    .buffer_stack_depth = 0
};

/* Signal handler for SIGSEGV */
static void crash_handler(int sig) {
    /* Use write() for async-signal safety */
    const char* msg1 = "\n============================================\n";
    const char* msg2 = "=== CRASH: Segmentation Fault ===\n";
    const char* msg3 = "============================================\n";
    write(2, msg1, strlen(msg1));
    write(2, msg2, strlen(msg2));
    write(2, msg3, strlen(msg3));

    /* Use fprintf for detailed output (technically unsafe but works in practice) */
    fprintf(stderr, "Phase: %s\n", crash_context.phase);

    if (crash_context.current_file) {
        fprintf(stderr, "File: %s\n", crash_context.current_file);
    }

    if (crash_context.current_word[0]) {
        fprintf(stderr, "Word: %s\n", crash_context.current_word);
    }

    if (crash_context.current_token[0]) {
        fprintf(stderr, "Token: %s\n", crash_context.current_token);
    }

    fprintf(stderr, "Type stack depth: %d\n", crash_context.type_stack_depth);
    fprintf(stderr, "Quotation stack depth: %d\n", crash_context.quot_stack_depth);
    fprintf(stderr, "Buffer stack depth: %d\n", crash_context.buffer_stack_depth);
    fprintf(stderr, "In quotation: %s\n",
            crash_context.buffer_stack_depth > 0 ? "YES" : "NO");
    fprintf(stderr, "============================================\n");
    fflush(stderr);

    /* Exit immediately to avoid double-crash */
    _exit(139);
}

/* Install crash handler */
void crash_handler_install(void) {
    signal(SIGSEGV, crash_handler);
}

/* Update context functions */
void crash_context_set_phase(const char* phase) {
    crash_context.phase = phase;
}

void crash_context_set_file(const char* file) {
    crash_context.current_file = file;
}

void crash_context_set_word(const char* word) {
    if (word) {
        strncpy(crash_context.current_word, word, 63);
        crash_context.current_word[63] = '\0';
    } else {
        crash_context.current_word[0] = '\0';
    }
}

void crash_context_set_token(const char* token) {
    if (token) {
        strncpy(crash_context.current_token, token, 63);
        crash_context.current_token[63] = '\0';
    } else {
        crash_context.current_token[0] = '\0';
    }
}

void crash_context_set_stacks(int type_depth, int quot_depth, int buffer_depth) {
    crash_context.type_stack_depth = type_depth;
    crash_context.quot_stack_depth = quot_depth;
    crash_context.buffer_stack_depth = buffer_depth;
}
