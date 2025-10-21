/*
 * Tests for dictionary.c
 */

#include "test_framework.h"
#include "dictionary.h"

int main(void) {
    TEST_SUITE("Dictionary Operations");

    dictionary_t* dict = dict_create();
    ASSERT_NOT_NULL(dict);

    /* Test type signature parsing */
    type_sig_t sig;
    ASSERT(parse_type_sig("i64 i64 -> i64", &sig));
    ASSERT_EQ(sig.input_count, 2);
    ASSERT_EQ(sig.output_count, 1);
    ASSERT_EQ(sig.inputs[0], TYPE_I64);
    ASSERT_EQ(sig.inputs[1], TYPE_I64);
    ASSERT_EQ(sig.outputs[0], TYPE_I64);

    ASSERT(parse_type_sig("i64 -> i64 i64", &sig));
    ASSERT_EQ(sig.input_count, 1);
    ASSERT_EQ(sig.output_count, 2);

    ASSERT(parse_type_sig("-> i64", &sig));
    ASSERT_EQ(sig.input_count, 0);
    ASSERT_EQ(sig.output_count, 1);

    /* Test adding words */
    type_sig_t add_sig;
    parse_type_sig("i64 i64 -> i64", &add_sig);
    ASSERT(dict_add(dict, "+", (void*)0x1000, NULL, 0, &add_sig, true, false, NULL));

    type_sig_t dup_sig;
    parse_type_sig("i64 -> i64 i64", &dup_sig);
    ASSERT(dict_add(dict, "dup", (void*)0x2000, NULL, 0, &dup_sig, true, false, NULL));

    /* Test lookup */
    dict_entry_t* entry = dict_lookup(dict, "+");
    ASSERT_NOT_NULL(entry);
    ASSERT_STR_EQ(entry->name, "+");
    ASSERT_EQ(entry->addr, (void*)0x1000);
    ASSERT(entry->is_primitive);

    entry = dict_lookup(dict, "dup");
    ASSERT_NOT_NULL(entry);
    ASSERT_STR_EQ(entry->name, "dup");

    entry = dict_lookup(dict, "nonexistent");
    ASSERT_NULL(entry);

    /* Test typed lookup (overload resolution) */
    type_id_t stack[8] = {TYPE_I64, TYPE_I64};
    entry = dict_lookup_typed(dict, "+", stack, 2);
    ASSERT_NOT_NULL(entry);
    ASSERT_STR_EQ(entry->name, "+");

    /* Add overloaded word */
    type_sig_t add_f64_sig;
    parse_type_sig("f64 f64 -> f64", &add_f64_sig);
    ASSERT(dict_add(dict, "+", (void*)0x3000, NULL, 0, &add_f64_sig, true, false, NULL));

    /* Lookup should now pick the right overload */
    type_id_t i64_stack[8] = {TYPE_I64, TYPE_I64};
    entry = dict_lookup_typed(dict, "+", i64_stack, 2);
    ASSERT_NOT_NULL(entry);
    ASSERT_EQ(entry->addr, (void*)0x1000);  /* Should get i64 version */

    type_id_t f64_stack[8] = {TYPE_F64, TYPE_F64};
    entry = dict_lookup_typed(dict, "+", f64_stack, 2);
    ASSERT_NOT_NULL(entry);
    ASSERT_EQ(entry->addr, (void*)0x3000);  /* Should get f64 version */

    dict_free(dict);

    TEST_SUMMARY();
}
