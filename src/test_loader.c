/*
 * March Language - Loader and Runner Tests
 */

#include "test_framework.h"
#include "compiler.h"
#include "loader.h"
#include "runner.h"
#include "database.h"
#include "dictionary.h"
#include <stdio.h>
#include <unistd.h>

/* Stub primitives (we link with real VM) */
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
    TEST_SUITE("Loader and Runner");

    const char* test_db = "test_loader.db";
    const char* schema_file = "../schema.sql";
    const char* test_source = "test_loader.march";

    /* Clean up */
    unlink(test_db);
    unlink(test_source);

    /* Setup: Create database and compile test words */
    march_db_t* db = db_open(test_db);
    ASSERT(db != NULL);
    ASSERT(db_init_schema(db, schema_file));

    dictionary_t* dict = dict_create();
    ASSERT(dict != NULL);

    compiler_t* comp = compiler_create(dict, db);
    ASSERT(comp != NULL);
    compiler_register_primitives(comp);

    /* Compile test words */
    FILE* f = fopen(test_source, "w");
    fprintf(f, ": five 5 ;\n");
    fprintf(f, ": ten 10 ;\n");
    fprintf(f, ": twenty 10 10 + ;\n");
    fclose(f);

    ASSERT(compiler_compile_file(comp, test_source));

    /* Test 1: Create loader */
    loader_t* loader = loader_create(db, dict);
    ASSERT(loader != NULL);
    ASSERT(loader->db == db);
    ASSERT(loader->dict == dict);
    ASSERT(loader->word_count == 0);

    /* Test 2: Load word 'five' */
    loaded_word_t* word = loader_load_word(loader, "five", "user");
    ASSERT(word != NULL);
    ASSERT(word->name != NULL);
    ASSERT_STR_EQ(word->name, "five");
    ASSERT(word->cells != NULL);
    ASSERT_EQ(word->cell_count, 2);  /* LIT(5) + EXIT */
    ASSERT(word->entry_point == (void*)word->cells);

    /* Test 3: Verify word is cached */
    ASSERT_EQ(loader->word_count, 1);
    loaded_word_t* cached = loader_find_word(loader, "five");
    ASSERT(cached == word);

    /* Test 4: Load again returns cached version */
    loaded_word_t* word2 = loader_load_word(loader, "five", "user");
    ASSERT(word2 == word);
    ASSERT_EQ(loader->word_count, 1);  /* Still only one word loaded */

    /* Test 5: Load multiple words */
    loaded_word_t* ten_word = loader_load_word(loader, "ten", "user");
    ASSERT(ten_word != NULL);
    ASSERT_EQ(loader->word_count, 2);

    /* Test 6: Get entry point */
    void* entry = loader_get_entry_point(loader, "five");
    ASSERT(entry == word->entry_point);

    /* Test 7: Create runner */
    runner_t* runner = runner_create(loader);
    ASSERT(runner != NULL);
    ASSERT(runner->loader == loader);

    /* Test 8: Execute 'five' */
    ASSERT(runner_execute(runner, "five"));

    /* Test 9: Check stack */
    int64_t stack[10];
    int depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 5);

    /* Test 10: Execute 'ten' */
    vm_init();  /* Reset VM */
    ASSERT(runner_execute(runner, "ten"));
    depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 10);

    /* Note: 'twenty' uses the + primitive which is stubbed in this test
     * Real primitive testing is done separately with full VM linkage */

    /* Clean up */
    runner_free(runner);
    loader_free(loader);
    compiler_free(comp);
    dict_free(dict);
    db_close(db);
    unlink(test_db);
    unlink(test_source);

    TEST_SUMMARY();
    return 0;
}
