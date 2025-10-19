/*
 * March Language - Quotation and Control Flow Tests
 *
 * Tests quotations and if statements with real VM execution
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
    TEST_SUITE("Quotations and Control Flow");

    const char* test_db = "test_quotations.db";
    const char* schema_file = "../schema.sql";
    const char* test_source = "test_quotations_source.march";

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
    fprintf(f, "-- Test if with true condition (1)\n");
    fprintf(f, ": test_if_true\n");
    fprintf(f, "  1 ( 42 ) ( 99 ) if\n");
    fprintf(f, ";\n");
    fprintf(f, "\n");
    fprintf(f, "-- Test if with false condition (0)\n");
    fprintf(f, ": test_if_false\n");
    fprintf(f, "  0 ( 42 ) ( 99 ) if\n");
    fprintf(f, ";\n");
    fclose(f);

    ASSERT(compiler_compile_file(comp, test_source));

    /* Create loader and runner */
    loader_t* loader = loader_create(db, dict);
    ASSERT(loader != NULL);

    runner_t* runner = runner_create(loader);
    ASSERT(runner != NULL);

    /* Test 1: Execute test_if_true - should return 42 */
    vm_init();
    ASSERT(runner_execute(runner, "test_if_true"));

    int64_t stack[10];
    int depth = runner_get_stack(runner, stack, 10);
    printf("DEBUG: Stack depth=%d, contents: ", depth);
    for (int i = 0; i < depth; i++) {
        printf("%lld ", (long long)stack[i]);
    }
    printf("\n");
    ASSERT_EQ(depth, 1);
    ASSERT_EQ(stack[0], 42);

    /* Test 2: Execute test_if_false - should return 99 */
    vm_init();
    ASSERT(runner_execute(runner, "test_if_false"));

    depth = runner_get_stack(runner, stack, 10);
    printf("DEBUG: Stack depth=%d, contents: ", depth);
    for (int i = 0; i < depth; i++) {
        printf("%lld ", (long long)stack[i]);
    }
    printf("\n");
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
