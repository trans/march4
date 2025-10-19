/*
 * March Language - Immediate Word Tests
 *
 * Tests true/false immediate words with real VM execution
 */

#include "test_framework.h"
#include "compiler.h"
#include "loader.h"
#include "runner.h"
#include "database.h"
#include "dictionary.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    TEST_SUITE("Immediate Words (true/false)");

    const char* test_db = "test_immediate.db";
    const char* schema_file = "../schema.sql";
    const char* test_source = "test_immediate_source.march";

    /* Clean up */
    unlink(test_db);
    unlink(test_source);

    /* Setup: Create database */
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
    fprintf(f, "-- Test true immediate word\n");
    fprintf(f, ": test_true\n");
    fprintf(f, "  true\n");
    fprintf(f, ";\n");
    fprintf(f, "\n");
    fprintf(f, "-- Test false immediate word\n");
    fprintf(f, ": test_false\n");
    fprintf(f, "  false\n");
    fprintf(f, ";\n");
    fprintf(f, "\n");
    fprintf(f, "-- Test true in if statement\n");
    fprintf(f, ": test_true_if\n");
    fprintf(f, "  true ( 42 ) ( 99 ) if\n");
    fprintf(f, ";\n");
    fprintf(f, "\n");
    fprintf(f, "-- Test false in if statement\n");
    fprintf(f, ": test_false_if\n");
    fprintf(f, "  false ( 42 ) ( 99 ) if\n");
    fprintf(f, ";\n");
    fclose(f);

    ASSERT(compiler_compile_file(comp, test_source));

    /* Create loader and runner */
    loader_t* loader = loader_create(db, dict);
    ASSERT(loader != NULL);

    runner_t* runner = runner_create(loader);
    ASSERT(runner != NULL);

    /* Test 1: Execute test_true - should return -1 */
    vm_init();
    ASSERT(runner_execute(runner, "test_true"));

    int64_t stack[10];
    int depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], -1);

    /* Test 2: Execute test_false - should return 0 */
    vm_init();
    ASSERT(runner_execute(runner, "test_false"));

    depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 0);

    /* Test 3: Execute test_true_if - should return 42 (true branch) */
    vm_init();
    ASSERT(runner_execute(runner, "test_true_if"));

    depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 42);

    /* Test 4: Execute test_false_if - should return 99 (false branch) */
    vm_init();
    ASSERT(runner_execute(runner, "test_false_if"));

    depth = runner_get_stack(runner, stack, 10);
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 99);

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
