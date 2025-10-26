/*
 * March Language - Debug/Trace System
 * Conditional debug output for development
 */

#ifndef MARCH_DEBUG_H
#define MARCH_DEBUG_H

#include <stdio.h>
#include <stdbool.h>

/* Debug categories (can be enabled/disabled independently) */
typedef enum {
    DEBUG_COMPILER  = 1 << 0,  /* Compiler operations */
    DEBUG_DICT      = 1 << 1,  /* Dictionary operations */
    DEBUG_TYPES     = 1 << 2,  /* Type checking */
    DEBUG_CID       = 1 << 3,  /* CID operations */
    DEBUG_LOADER    = 1 << 4,  /* Loader/linking */
    DEBUG_RUNTIME   = 1 << 5,  /* Runtime execution */
    DEBUG_DB        = 1 << 6,  /* Database operations */
    DEBUG_ALL       = 0xFF     /* All categories */
} debug_category_t;

/* Global debug flags (can be set via environment or command line) */
extern unsigned int debug_flags;

/* Initialize debug system from environment variable MARCH_DEBUG */
void debug_init(void);

/* Enable/disable debug categories */
void debug_enable(debug_category_t category);
void debug_disable(debug_category_t category);
bool debug_enabled(debug_category_t category);

/* Debug print macros */
#define DEBUG(category, fmt, ...) \
    do { \
        if (debug_enabled(category)) { \
            fprintf(stderr, "[%s] " fmt "\n", #category, ##__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_COMPILER(fmt, ...) DEBUG(DEBUG_COMPILER, fmt, ##__VA_ARGS__)
#define DEBUG_DICT(fmt, ...)     DEBUG(DEBUG_DICT, fmt, ##__VA_ARGS__)
#define DEBUG_TYPES(fmt, ...)    DEBUG(DEBUG_TYPES, fmt, ##__VA_ARGS__)
#define DEBUG_CID(fmt, ...)      DEBUG(DEBUG_CID, fmt, ##__VA_ARGS__)
#define DEBUG_LOADER(fmt, ...)   DEBUG(DEBUG_LOADER, fmt, ##__VA_ARGS__)
#define DEBUG_RUNTIME(fmt, ...)  DEBUG(DEBUG_RUNTIME, fmt, ##__VA_ARGS__)
#define DEBUG_DB(fmt, ...)       DEBUG(DEBUG_DB, fmt, ##__VA_ARGS__)

/* Helper to dump type stack */
void debug_dump_type_stack(const char* label, void* type_stack, int depth);

/* Helper to dump dictionary stats */
void debug_dump_dict_stats(void* dict);

/* ============================================================================ */
/* Crash Handler - Track context for better segfault debugging */
/* ============================================================================ */

/* Crash context tracking */
typedef struct {
    const char* phase;           /* "init", "register", "compile", "execute" */
    const char* current_file;    /* Source file being compiled */
    char current_word[64];       /* Word being defined (fixed buffer) */
    char current_token[64];      /* Token being processed (fixed buffer) */
    int type_stack_depth;
    int quot_stack_depth;
    int buffer_stack_depth;
} crash_context_t;

extern crash_context_t crash_context;

/* Install signal handler for SIGSEGV */
void crash_handler_install(void);

/* Update crash context (call from compiler) */
void crash_context_set_phase(const char* phase);
void crash_context_set_file(const char* file);
void crash_context_set_word(const char* word);
void crash_context_set_token(const char* token);
void crash_context_set_stacks(int type_depth, int quot_depth, int buffer_depth);

#endif /* MARCH_DEBUG_H */
