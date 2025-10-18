/*
 * March Language - Primitives Registration Tests
 */

#include "test_framework.h"
#include "primitives.h"
#include "dictionary.h"
#include <stdio.h>

/* Stub primitives (we don't link to real assembly for this test) */
void op_dup(void) {}
void op_drop(void) {}
void op_swap(void) {}
void op_over(void) {}
void op_rot(void) {}
void op_add(void) {}
void op_sub(void) {}
void op_mul(void) {}
void op_div(void) {}
void op_mod(void) {}
void op_eq(void) {}
void op_ne(void) {}
void op_lt(void) {}
void op_gt(void) {}
void op_le(void) {}
void op_ge(void) {}
void op_and(void) {}
void op_or(void) {}
void op_xor(void) {}
void op_not(void) {}
void op_lshift(void) {}
void op_rshift(void) {}
void op_arshift(void) {}
void op_land(void) {}
void op_lor(void) {}
void op_lnot(void) {}
void op_zerop(void) {}
void op_zerogt(void) {}
void op_zerolt(void) {}
void op_fetch(void) {}
void op_store(void) {}
void op_cfetch(void) {}
void op_cstore(void) {}
void op_tor(void) {}
void op_fromr(void) {}
void op_rfetch(void) {}
void op_rdrop(void) {}
void op_twotor(void) {}
void op_twofromr(void) {}

int main(void) {
    TEST_SUITE("Primitive Registration");

    dictionary_t* dict = dict_create();
    ASSERT(dict != NULL);

    /* Register all primitives */
    register_primitives(dict);

    /* Test stack ops */
    dict_entry_t* entry;
    entry = dict_lookup(dict, "dup");
    ASSERT(entry != NULL);
    ASSERT(entry->is_primitive);
    ASSERT_EQ(entry->signature.input_count, 1);
    ASSERT_EQ(entry->signature.output_count, 2);

    entry = dict_lookup(dict, "drop");
    ASSERT(entry != NULL);
    ASSERT_EQ(entry->signature.input_count, 1);
    ASSERT_EQ(entry->signature.output_count, 0);

    entry = dict_lookup(dict, "swap");
    ASSERT(entry != NULL);
    ASSERT_EQ(entry->signature.input_count, 2);
    ASSERT_EQ(entry->signature.output_count, 2);

    /* Test arithmetic */
    entry = dict_lookup(dict, "+");
    ASSERT(entry != NULL);
    ASSERT_EQ(entry->signature.input_count, 2);
    ASSERT_EQ(entry->signature.output_count, 1);
    ASSERT_EQ(entry->signature.inputs[0], TYPE_I64);
    ASSERT_EQ(entry->signature.outputs[0], TYPE_I64);

    entry = dict_lookup(dict, "-");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "*");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "/");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "mod");
    ASSERT(entry != NULL);

    /* Test comparisons */
    entry = dict_lookup(dict, "=");
    ASSERT(entry != NULL);
    ASSERT_EQ(entry->signature.input_count, 2);
    ASSERT_EQ(entry->signature.output_count, 1);
    ASSERT_EQ(entry->signature.outputs[0], TYPE_BOOL);

    entry = dict_lookup(dict, "<>");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "<");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, ">");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "<=");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, ">=");
    ASSERT(entry != NULL);

    /* Test bitwise */
    entry = dict_lookup(dict, "and");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "or");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "xor");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "not");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "<<");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, ">>");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, ">>>");
    ASSERT(entry != NULL);

    /* Test logical */
    entry = dict_lookup(dict, "land");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "lor");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "lnot");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "0=");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "0>");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "0<");
    ASSERT(entry != NULL);

    /* Test memory */
    entry = dict_lookup(dict, "@");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "!");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "c@");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "c!");
    ASSERT(entry != NULL);

    /* Test return stack */
    entry = dict_lookup(dict, ">r");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "r>");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "r@");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "rdrop");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "2>r");
    ASSERT(entry != NULL);
    entry = dict_lookup(dict, "2r>");
    ASSERT(entry != NULL);

    /* Verify we have exactly 39 primitives registered */
    int count = 0;
    for (size_t i = 0; i < dict->bucket_count; i++) {
        dict_entry_t* e = dict->buckets[i];
        while (e) {
            if (e->is_primitive) count++;
            e = e->next;
        }
    }
    ASSERT_EQ(count, 39);

    dict_free(dict);

    TEST_SUMMARY();
    return 0;
}
