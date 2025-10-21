/*
 * March Language - Primitive Registration
 */

#include "primitives.h"
#include "types.h"
#include <stdio.h>

/* ============================================================================ */
/* Primitive Dispatch Table */
/* ============================================================================ */
/* Maps primitive IDs to their runtime addresses.
 * This is a static table compiled into the binary - no database needed!
 */
void* primitive_dispatch_table[256] = {
    [PRIM_ADD]      = &op_add,
    [PRIM_SUB]      = &op_sub,
    [PRIM_MUL]      = &op_mul,
    [PRIM_DIV]      = &op_div,
    [PRIM_MOD]      = &op_mod,
    [PRIM_DUP]      = &op_dup,
    [PRIM_DROP]     = &op_drop,
    [PRIM_SWAP]     = &op_swap,
    [PRIM_OVER]     = &op_over,
    [PRIM_ROT]      = &op_rot,
    [PRIM_EQ]       = &op_eq,
    [PRIM_NE]       = &op_ne,
    [PRIM_LT]       = &op_lt,
    [PRIM_GT]       = &op_gt,
    [PRIM_LE]       = &op_le,
    [PRIM_GE]       = &op_ge,
    [PRIM_AND]      = &op_and,
    [PRIM_OR]       = &op_or,
    [PRIM_XOR]      = &op_xor,
    [PRIM_NOT]      = &op_not,
    [PRIM_LSHIFT]   = &op_lshift,
    [PRIM_RSHIFT]   = &op_rshift,
    [PRIM_ARSHIFT]  = &op_arshift,
    [PRIM_LAND]     = &op_land,
    [PRIM_LOR]      = &op_lor,
    [PRIM_LNOT]     = &op_lnot,
    [PRIM_ZEROP]    = &op_zerop,
    [PRIM_ZEROGT]   = &op_zerogt,
    [PRIM_ZEROLT]   = &op_zerolt,
    [PRIM_FETCH]    = &op_fetch,
    [PRIM_STORE]    = &op_store,
    [PRIM_CFETCH]   = &op_cfetch,
    [PRIM_CSTORE]   = &op_cstore,
    [PRIM_TOR]      = &op_tor,
    [PRIM_FROMR]    = &op_fromr,
    [PRIM_RFETCH]   = &op_rfetch,
    [PRIM_RDROP]    = &op_rdrop,
    [PRIM_TWOTOR]   = &op_twotor,
    [PRIM_TWOFROMR] = &op_twofromr,
    [PRIM_BRANCH]   = &op_branch,
    [PRIM_0BRANCH]  = &op_0branch,
    [PRIM_EXECUTE]  = &op_execute,
};

/* ============================================================================ */
/* Primitive Registration */
/* ============================================================================ */

/* Helper macro to register a primitive in the dictionary */
#define REG_PRIM(name_str, prim_id, op_func, type_str) \
    do { \
        parse_type_sig(type_str, &sig); \
        dict_add(dict, name_str, &op_func, NULL, prim_id, &sig, true, false, NULL); \
    } while(0)

void register_primitives(dictionary_t* dict) {
    type_sig_t sig;

    /* Stack ops */
    REG_PRIM("dup", PRIM_DUP, op_dup, "i64 -> i64 i64");
    REG_PRIM("drop", PRIM_DROP, op_drop, "i64 ->");
    REG_PRIM("swap", PRIM_SWAP, op_swap, "i64 i64 -> i64 i64");
    REG_PRIM("over", PRIM_OVER, op_over, "i64 i64 -> i64 i64 i64");
    REG_PRIM("rot", PRIM_ROT, op_rot, "i64 i64 i64 -> i64 i64 i64");

    /* Arithmetic */
    REG_PRIM("+", PRIM_ADD, op_add, "i64 i64 -> i64");
    REG_PRIM("-", PRIM_SUB, op_sub, "i64 i64 -> i64");
    REG_PRIM("*", PRIM_MUL, op_mul, "i64 i64 -> i64");
    REG_PRIM("/", PRIM_DIV, op_div, "i64 i64 -> i64");
    REG_PRIM("mod", PRIM_MOD, op_mod, "i64 i64 -> i64");

    /* Comparisons */
    REG_PRIM("=", PRIM_EQ, op_eq, "i64 i64 -> bool");
    REG_PRIM("<>", PRIM_NE, op_ne, "i64 i64 -> bool");
    REG_PRIM("<", PRIM_LT, op_lt, "i64 i64 -> bool");
    REG_PRIM(">", PRIM_GT, op_gt, "i64 i64 -> bool");
    REG_PRIM("<=", PRIM_LE, op_le, "i64 i64 -> bool");
    REG_PRIM(">=", PRIM_GE, op_ge, "i64 i64 -> bool");

    /* Bitwise */
    REG_PRIM("and", PRIM_AND, op_and, "i64 i64 -> i64");
    REG_PRIM("or", PRIM_OR, op_or, "i64 i64 -> i64");
    REG_PRIM("xor", PRIM_XOR, op_xor, "i64 i64 -> i64");
    REG_PRIM("not", PRIM_NOT, op_not, "i64 -> i64");
    REG_PRIM("<<", PRIM_LSHIFT, op_lshift, "i64 i64 -> i64");
    REG_PRIM(">>", PRIM_RSHIFT, op_rshift, "i64 i64 -> i64");
    REG_PRIM(">>>", PRIM_ARSHIFT, op_arshift, "i64 i64 -> i64");

    /* Logical */
    REG_PRIM("land", PRIM_LAND, op_land, "bool bool -> bool");
    REG_PRIM("lor", PRIM_LOR, op_lor, "bool bool -> bool");
    REG_PRIM("lnot", PRIM_LNOT, op_lnot, "bool -> bool");
    REG_PRIM("0=", PRIM_ZEROP, op_zerop, "i64 -> bool");
    REG_PRIM("0>", PRIM_ZEROGT, op_zerogt, "i64 -> bool");
    REG_PRIM("0<", PRIM_ZEROLT, op_zerolt, "i64 -> bool");

    /* Memory */
    REG_PRIM("@", PRIM_FETCH, op_fetch, "ptr -> i64");
    REG_PRIM("!", PRIM_STORE, op_store, "i64 ptr ->");
    REG_PRIM("c@", PRIM_CFETCH, op_cfetch, "ptr -> i64");
    REG_PRIM("c!", PRIM_CSTORE, op_cstore, "i64 ptr ->");

    /* Return stack */
    REG_PRIM(">r", PRIM_TOR, op_tor, "i64 ->");
    REG_PRIM("r>", PRIM_FROMR, op_fromr, "-> i64");
    REG_PRIM("r@", PRIM_RFETCH, op_rfetch, "-> i64");
    REG_PRIM("rdrop", PRIM_RDROP, op_rdrop, "->");
    REG_PRIM("2>r", PRIM_TWOTOR, op_twotor, "i64 i64 ->");
    REG_PRIM("2r>", PRIM_TWOFROMR, op_twofromr, "-> i64 i64");

    /* Control flow */
    REG_PRIM("branch", PRIM_BRANCH, op_branch, "->");
    REG_PRIM("0branch", PRIM_0BRANCH, op_0branch, "i64 ->");

    /* Quotation execution */
    REG_PRIM("execute", PRIM_EXECUTE, op_execute, "ptr ->");
}

#undef REG_PRIM
