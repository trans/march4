/*
 * Tests for cells.c
 */

#include "test_framework.h"
#include "cells.h"

int main(void) {
    TEST_SUITE("Cell Encoding/Decoding");

    /* Test LIT encoding */
    cell_t lit5 = encode_lit(5);
    ASSERT_EQ(lit5 & 0x3, TAG_LIT);
    ASSERT_EQ(decode_lit(lit5), 5);
    ASSERT(is_lit(lit5));

    cell_t lit_neg = encode_lit(-42);
    ASSERT_EQ(decode_lit(lit_neg), -42);
    ASSERT(is_lit(lit_neg));

    /* Test EXIT encoding */
    cell_t exit_cell = encode_exit();
    ASSERT_EQ(exit_cell, 0ULL);
    ASSERT(is_exit(exit_cell));
    ASSERT(is_xt(exit_cell));  /* EXIT is XT(0) */

    /* Test XT encoding */
    void* addr = (void*)0x12345678;
    cell_t xt = encode_xt(addr);
    ASSERT_EQ(xt & 0x3, TAG_XT);
    ASSERT_EQ(decode_xt(xt), addr);
    ASSERT(is_xt(xt));

    /* Test LST encoding */
    cell_t lst = encode_lst(42);
    ASSERT_EQ(lst & 0x7, TAG_LST);  /* Need 3 bits */
    ASSERT_EQ(decode_lst(lst), 42);
    ASSERT(is_lst(lst));

    /* Test LNT encoding */
    cell_t lnt = encode_lnt(5);
    ASSERT_EQ(lnt & 0x7, TAG_LNT);  /* 110 binary */
    ASSERT_EQ(decode_lnt(lnt), 5);
    ASSERT(is_lnt(lnt));

    /* Test tag decoding */
    ASSERT_EQ(decode_tag(encode_xt((void*)0x100)), TAG_XT);
    ASSERT_EQ(decode_tag(encode_lit(123)), TAG_LIT);
    ASSERT_EQ(decode_tag(encode_lst(1)), TAG_LST);
    ASSERT_EQ(decode_tag(encode_lnt(3)), TAG_LNT);

    /* Test cell buffer */
    cell_buffer_t* buf = cell_buffer_create();
    ASSERT_NOT_NULL(buf);
    ASSERT_EQ(buf->count, 0);

    cell_buffer_append(buf, encode_lit(10));
    cell_buffer_append(buf, encode_lit(20));
    ASSERT_EQ(buf->count, 2);
    ASSERT_EQ(decode_lit(buf->cells[0]), 10);
    ASSERT_EQ(decode_lit(buf->cells[1]), 20);

    cell_buffer_clear(buf);
    ASSERT_EQ(buf->count, 0);

    cell_buffer_free(buf);

    TEST_SUMMARY();
}
