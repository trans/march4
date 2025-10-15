/*
 * March VM Test Program
 * Tests the inner interpreter with a simple program
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// VM interface
extern void vm_init(void);
extern void vm_run(uint64_t* code_ptr);
extern void vm_halt(void);
extern uint64_t* vm_get_dsp(void);
extern uint64_t* vm_get_rsp(void);

// Primitive operations
extern void op_add(void);
extern void op_sub(void);
extern void op_mul(void);
extern void op_dup(void);
extern void op_drop(void);
extern void op_swap(void);
extern void op_eq(void);

// Cell tag constants
#define TAG_XT   0  // Execute word
#define TAG_EXIT 1  // Return
#define TAG_LIT  2  // Literal
#define TAG_EXT  3  // Extended

// Helper to create tagged cells
static inline uint64_t make_xt(void* addr) {
    return ((uint64_t)addr & ~0x3ULL) | TAG_XT;
}

static inline uint64_t make_exit(void) {
    return TAG_EXIT;
}

static inline uint64_t make_lit(void) {
    return TAG_LIT;
}

// Test 1: Simple arithmetic: 5 3 + (should leave 8)
void test_simple_add(void) {
    printf("\n=== Test 1: Simple Addition ===\n");
    printf("Program: 5 3 +\n");

    uint64_t program[] = {
        make_lit(), 5,      // Push 5
        make_lit(), 3,      // Push 3
        make_xt(op_add),    // Add them
        make_exit()         // Exit
    };

    vm_init();
    vm_run(program);

    uint64_t* dsp = vm_get_dsp();
    uint64_t result = dsp[0];

    printf("Result: %lu (expected 8)\n", result);
    printf("Test %s\n", result == 8 ? "PASSED" : "FAILED");
}

// Test 2: Stack manipulation: 10 dup + (10 duplicated, then added = 20)
void test_dup_add(void) {
    printf("\n=== Test 2: Dup and Add ===\n");
    printf("Program: 10 dup +\n");

    uint64_t program[] = {
        make_lit(), 10,     // Push 10
        make_xt(op_dup),    // Duplicate it (10 10)
        make_xt(op_add),    // Add them (20)
        make_exit()
    };

    vm_init();
    vm_run(program);

    uint64_t* dsp = vm_get_dsp();
    uint64_t result = dsp[0];

    printf("Result: %lu (expected 20)\n", result);
    printf("Test %s\n", result == 20 ? "PASSED" : "FAILED");
}

// Test 3: Multiple operations: 7 3 - 2 * (should be 8)
void test_complex(void) {
    printf("\n=== Test 3: Complex Expression ===\n");
    printf("Program: 7 3 - 2 *\n");
    printf("Expected: (7 - 3) * 2 = 8\n");

    uint64_t program[] = {
        make_lit(), 7,      // Push 7
        make_lit(), 3,      // Push 3
        make_xt(op_sub),    // Subtract (4)
        make_lit(), 2,      // Push 2
        make_xt(op_mul),    // Multiply (8)
        make_exit()
    };

    vm_init();
    vm_run(program);

    uint64_t* dsp = vm_get_dsp();
    uint64_t result = dsp[0];

    printf("Result: %lu (expected 8)\n", result);
    printf("Test %s\n", result == 8 ? "PASSED" : "FAILED");
}

// Test 4: Comparison: 5 5 = (should return -1 for true)
void test_comparison(void) {
    printf("\n=== Test 4: Equality Test ===\n");
    printf("Program: 5 5 =\n");

    uint64_t program[] = {
        make_lit(), 5,      // Push 5
        make_lit(), 5,      // Push 5
        make_xt(op_eq),     // Compare (should be -1/true)
        make_exit()
    };

    vm_init();
    vm_run(program);

    uint64_t* dsp = vm_get_dsp();
    int64_t result = (int64_t)dsp[0];

    printf("Result: %ld (expected -1)\n", result);
    printf("Test %s\n", result == -1 ? "PASSED" : "FAILED");
}

// Test 5: Swap: 10 20 swap - (should be 10, since 20 - 10)
void test_swap(void) {
    printf("\n=== Test 5: Swap Test ===\n");
    printf("Program: 10 20 swap -\n");
    printf("Expected: 20 - 10 = 10\n");

    uint64_t program[] = {
        make_lit(), 10,     // Push 10
        make_lit(), 20,     // Push 20
        make_xt(op_swap),   // Swap (now 10 on top, 20 below)
        make_xt(op_sub),    // Subtract: 20 - 10 = 10
        make_exit()
    };

    vm_init();
    vm_run(program);

    uint64_t* dsp = vm_get_dsp();
    uint64_t result = dsp[0];

    printf("Result: %lu (expected 10)\n", result);
    printf("Test %s\n", result == 10 ? "PASSED" : "FAILED");
}

int main(void) {
    printf("March VM Test Suite\n");
    printf("===================\n");

    test_simple_add();
    test_dup_add();
    test_complex();
    test_comparison();
    test_swap();

    printf("\n===================\n");
    printf("All tests complete!\n");

    return 0;
}
