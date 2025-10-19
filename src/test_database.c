/*
 * March Language - Database Tests
 */

#include "test_framework.h"
#include "database.h"
#include "cells.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    TEST_SUITE("Database Operations");

    const char* test_db = "test_march.db";
    const char* schema_file = "../schema.sql";

    /* Clean up any existing test database */
    unlink(test_db);

    /* Test 1: Open database */
    march_db_t* db = db_open(test_db);
    ASSERT(db != NULL);
    ASSERT(db->db != NULL);
    ASSERT_STR_EQ(db->filename, test_db);

    /* Test 2: Initialize schema */
    ASSERT(db_init_schema(db, schema_file));

    /* Test 3: Compute SHA256 */
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    char* hash = compute_sha256(test_data, 4);
    ASSERT(hash != NULL);
    ASSERT_EQ(strlen(hash), 64);  /* SHA256 = 64 hex chars */
    free(hash);

    /* Test 4: Store word with simple cells */
    cell_t cells1[] = {
        encode_lit(5),
        encode_exit()
    };
    ASSERT(db_store_word(db, "five", "test", (uint8_t*)cells1, 2, "-> i64", "5"));

    /* Test 5: Load word back */
    size_t count = 0;
    uint64_t* loaded = db_load_word(db, "five", "test", &count);
    ASSERT(loaded != NULL);
    ASSERT_EQ(count, 2);
    ASSERT_EQ(loaded[0], cells1[0]);
    ASSERT_EQ(loaded[1], cells1[1]);
    free(loaded);

    /* Test 6: Store word with multiple cells */
    cell_t cells2[] = {
        encode_lit(10),
        encode_lit(20),
        encode_lit(30),
        encode_exit()
    };
    ASSERT(db_store_word(db, "three_nums", "test", (uint8_t*)cells2, 4, "-> i64 i64 i64", "10 20 30"));

    /* Test 7: Load multi-cell word */
    loaded = db_load_word(db, "three_nums", "test", &count);
    ASSERT(loaded != NULL);
    ASSERT_EQ(count, 4);
    ASSERT_EQ(loaded[0], cells2[0]);
    ASSERT_EQ(loaded[1], cells2[1]);
    ASSERT_EQ(loaded[2], cells2[2]);
    ASSERT_EQ(loaded[3], cells2[3]);
    free(loaded);

    /* Test 8: Store word with LNT */
    cell_t cells3[] = {
        encode_lnt(3),
        1, 2, 3,  /* Raw 64-bit values */
        encode_exit()
    };
    ASSERT(db_store_word(db, "lnt_test", "test", (uint8_t*)cells3, 5, "-> i64 i64 i64", "1 2 3"));

    /* Test 9: Load LNT word */
    loaded = db_load_word(db, "lnt_test", "test", &count);
    ASSERT(loaded != NULL);
    ASSERT_EQ(count, 5);
    ASSERT(is_lnt(loaded[0]));
    ASSERT_EQ(decode_lnt(loaded[0]), 3);
    ASSERT_EQ(loaded[1], 1);
    ASSERT_EQ(loaded[2], 2);
    ASSERT_EQ(loaded[3], 3);
    ASSERT(is_exit(loaded[4]));
    free(loaded);

    /* Test 10: Store duplicate word (same CID, new entry) */
    ASSERT(db_store_word(db, "five_copy", "test", (uint8_t*)cells1, 2, "-> i64", "5"));

    /* Test 11: Load by different name (same cells) */
    loaded = db_load_word(db, "five_copy", "test", &count);
    ASSERT(loaded != NULL);
    ASSERT_EQ(count, 2);
    ASSERT_EQ(loaded[0], cells1[0]);
    ASSERT_EQ(loaded[1], cells1[1]);
    free(loaded);

    /* Test 12: Load non-existent word returns NULL */
    loaded = db_load_word(db, "nonexistent", "test", &count);
    ASSERT(loaded == NULL);

    /* Test 13: Store word with default namespace */
    ASSERT(db_store_word(db, "default_ns", NULL, (uint8_t*)cells1, 2, "-> i64", "5"));

    /* Test 14: Load from default namespace */
    loaded = db_load_word(db, "default_ns", NULL, &count);
    ASSERT(loaded != NULL);
    ASSERT_EQ(count, 2);
    free(loaded);

    /* Clean up */
    db_close(db);
    unlink(test_db);

    TEST_SUMMARY();
    return 0;
}
