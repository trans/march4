/*
 * March Language - Compiler Tests
 */

#include "test_framework.h"
#include "compiler.h"
#include "database.h"
#include "dictionary.h"
#include "cells.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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
void op_branch(void) {}
void op_0branch(void) {}

int main(void) {
    TEST_SUITE("One-Pass Compiler");

    const char* test_db = "test_compiler.db";
    const char* schema_file = "../schema.sql";

    /* Clean up */
    unlink(test_db);

    /* Test 1: Create compiler */
    march_db_t* db = db_open(test_db);
    ASSERT(db != NULL);
    ASSERT(db_init_schema(db, schema_file));

    dictionary_t* dict = dict_create();
    ASSERT(dict != NULL);

    compiler_t* comp = compiler_create(dict, db);
    ASSERT(comp != NULL);
    ASSERT(comp->dict == dict);
    ASSERT(comp->db == db);
    ASSERT(comp->type_stack_depth == 0);
    ASSERT(comp->cells != NULL);

    /* Test 2: Register primitives */
    compiler_register_primitives(comp);
    dict_entry_t* entry = dict_lookup(dict, "+");
    ASSERT(entry != NULL);

    /* Test 3: Create test source file */
    const char* test_source = "test_source.march";
    FILE* f = fopen(test_source, "w");
    ASSERT(f != NULL);
    fprintf(f, "-- Test file\n");
    fprintf(f, ": five 5 ;\n");
    fprintf(f, ": ten 10 ;\n");
    fprintf(f, ": fifteen five ten + ;\n");
    fclose(f);

    /* Test 4: Compile file */
    ASSERT(compiler_compile_file(comp, test_source));

    /* Test 5: Verify 'five' was stored */
    size_t count = 0;
    uint64_t* cells = db_load_word(db, "five", "user", &count);
    ASSERT(cells != NULL);
    ASSERT(count == 2);  /* LIT(5) + EXIT */
    ASSERT(is_lit(cells[0]));
    ASSERT_EQ(decode_lit(cells[0]), 5);
    ASSERT(is_exit(cells[1]));
    free(cells);

    /* Test 6: Verify 'ten' was stored */
    cells = db_load_word(db, "ten", "user", &count);
    ASSERT(cells != NULL);
    ASSERT(count == 2);  /* LIT(10) + EXIT */
    ASSERT(is_lit(cells[0]));
    ASSERT_EQ(decode_lit(cells[0]), 10);
    ASSERT(is_exit(cells[1]));
    free(cells);

    /* Test 7: Verify 'fifteen' was stored (will have XT placeholders for now) */
    cells = db_load_word(db, "fifteen", "user", &count);
    ASSERT(cells != NULL);
    /* fifteen = XT(five) XT(ten) XT(+) EXIT = 4 cells */
    ASSERT(count == 4);
    free(cells);

    /* Test 8: Test type stack with verbose output */
    unlink(test_source);
    f = fopen(test_source, "w");
    fprintf(f, ": nums 1 2 3 4 5 ;\n");
    fclose(f);

    comp->verbose = true;
    ASSERT(compiler_compile_file(comp, test_source));

    cells = db_load_word(db, "nums", "user", &count);
    ASSERT(cells != NULL);
    ASSERT(count == 6);  /* 5 LITs + EXIT */
    for (int i = 0; i < 5; i++) {
        ASSERT(is_lit(cells[i]));
        ASSERT_EQ(decode_lit(cells[i]), i + 1);
    }
    ASSERT(is_exit(cells[5]));
    free(cells);

    /* Clean up */
    compiler_free(comp);
    dict_free(dict);
    db_close(db);
    unlink(test_db);
    unlink(test_source);

    TEST_SUMMARY();
    return 0;
}
