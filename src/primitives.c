/*
 * March Language - Primitive Registration
 */

#include "primitives.h"
#include <stdio.h>

void register_primitives(dictionary_t* dict) {
    type_sig_t sig;

    /* Stack ops */
    parse_type_sig("i64 -> i64 i64", &sig);
    dict_add(dict, "dup", &op_dup, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 ->", &sig);
    dict_add(dict, "drop", &op_drop, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 i64 -> i64 i64", &sig);
    dict_add(dict, "swap", &op_swap, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 i64 -> i64 i64 i64", &sig);
    dict_add(dict, "over", &op_over, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 i64 i64 -> i64 i64 i64", &sig);
    dict_add(dict, "rot", &op_rot, NULL, &sig, true, false, NULL);

    /* Arithmetic */
    parse_type_sig("i64 i64 -> i64", &sig);
    dict_add(dict, "+", &op_add, NULL, &sig, true, false, NULL);
    dict_add(dict, "-", &op_sub, NULL, &sig, true, false, NULL);
    dict_add(dict, "*", &op_mul, NULL, &sig, true, false, NULL);
    dict_add(dict, "/", &op_div, NULL, &sig, true, false, NULL);
    dict_add(dict, "mod", &op_mod, NULL, &sig, true, false, NULL);

    /* Comparisons */
    parse_type_sig("i64 i64 -> bool", &sig);
    dict_add(dict, "=", &op_eq, NULL, &sig, true, false, NULL);
    dict_add(dict, "<>", &op_ne, NULL, &sig, true, false, NULL);
    dict_add(dict, "<", &op_lt, NULL, &sig, true, false, NULL);
    dict_add(dict, ">", &op_gt, NULL, &sig, true, false, NULL);
    dict_add(dict, "<=", &op_le, NULL, &sig, true, false, NULL);
    dict_add(dict, ">=", &op_ge, NULL, &sig, true, false, NULL);

    /* Bitwise */
    parse_type_sig("i64 i64 -> i64", &sig);
    dict_add(dict, "and", &op_and, NULL, &sig, true, false, NULL);
    dict_add(dict, "or", &op_or, NULL, &sig, true, false, NULL);
    dict_add(dict, "xor", &op_xor, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 -> i64", &sig);
    dict_add(dict, "not", &op_not, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 i64 -> i64", &sig);
    dict_add(dict, "<<", &op_lshift, NULL, &sig, true, false, NULL);
    dict_add(dict, ">>", &op_rshift, NULL, &sig, true, false, NULL);
    dict_add(dict, ">>>", &op_arshift, NULL, &sig, true, false, NULL);

    /* Logical */
    parse_type_sig("bool bool -> bool", &sig);
    dict_add(dict, "land", &op_land, NULL, &sig, true, false, NULL);
    dict_add(dict, "lor", &op_lor, NULL, &sig, true, false, NULL);

    parse_type_sig("bool -> bool", &sig);
    dict_add(dict, "lnot", &op_lnot, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 -> bool", &sig);
    dict_add(dict, "0=", &op_zerop, NULL, &sig, true, false, NULL);
    dict_add(dict, "0>", &op_zerogt, NULL, &sig, true, false, NULL);
    dict_add(dict, "0<", &op_zerolt, NULL, &sig, true, false, NULL);

    /* Memory */
    parse_type_sig("ptr -> i64", &sig);
    dict_add(dict, "@", &op_fetch, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 ptr ->", &sig);
    dict_add(dict, "!", &op_store, NULL, &sig, true, false, NULL);

    parse_type_sig("ptr -> i64", &sig);
    dict_add(dict, "c@", &op_cfetch, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 ptr ->", &sig);
    dict_add(dict, "c!", &op_cstore, NULL, &sig, true, false, NULL);

    /* Return stack */
    parse_type_sig("i64 ->", &sig);
    dict_add(dict, ">r", &op_tor, NULL, &sig, true, false, NULL);

    parse_type_sig("-> i64", &sig);
    dict_add(dict, "r>", &op_fromr, NULL, &sig, true, false, NULL);

    parse_type_sig("-> i64", &sig);
    dict_add(dict, "r@", &op_rfetch, NULL, &sig, true, false, NULL);

    parse_type_sig("->", &sig);
    dict_add(dict, "rdrop", &op_rdrop, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 i64 ->", &sig);
    dict_add(dict, "2>r", &op_twotor, NULL, &sig, true, false, NULL);

    parse_type_sig("-> i64 i64", &sig);
    dict_add(dict, "2r>", &op_twofromr, NULL, &sig, true, false, NULL);

    /* Control flow */
    parse_type_sig("->", &sig);
    dict_add(dict, "branch", &op_branch, NULL, &sig, true, false, NULL);

    parse_type_sig("i64 ->", &sig);
    dict_add(dict, "0branch", &op_0branch, NULL, &sig, true, false, NULL);

    /* Quotation execution */
    parse_type_sig("ptr ->", &sig);
    dict_add(dict, "execute", &op_execute, NULL, &sig, true, false, NULL);
}
