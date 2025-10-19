/*
 * March Language - Core Type Definitions
 * Common types and constants used throughout the compiler
 */

#ifndef MARCH_TYPES_H
#define MARCH_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Cell tags - variable-bit encoding */
#define TAG_XT   0x0  /* 00  - Execute word (EXIT if addr=0) */
#define TAG_LIT  0x1  /* 01  - Immediate 62-bit signed literal */
#define TAG_LST  0x2  /* 010 - Symbol ID literal */
#define TAG_LNT  0x6  /* 110 - Next N literals (raw 64-bit values) */

/* Cell type */
typedef uint64_t cell_t;

/* Type identifiers for type checking */
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_I64,      /* 64-bit signed integer */
    TYPE_U64,      /* 64-bit unsigned integer */
    TYPE_F64,      /* 64-bit float */
    TYPE_PTR,      /* Pointer/address */
    TYPE_BOOL,     /* Boolean */
    TYPE_STR,      /* String reference (CID) */
    TYPE_ANY,      /* Polymorphic type variable */
} type_id_t;

/* Type stack entry (compile-time only) */
typedef struct {
    type_id_t type;
    int depth;  /* For debugging */
} type_stack_entry_t;

/* Maximum sizes */
#define MAX_TYPE_STACK    256
#define MAX_WORD_NAME     64
#define MAX_TYPE_SIG      256
#define MAX_CELL_STREAM   4096

#endif /* MARCH_TYPES_H */
