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

// Cell tag constants - New 4-tag variable-bit encoding
#define TAG_XT   0x0  // Execute word (EXIT if addr=0)
#define TAG_LIT  0x1  // Immediate 62-bit literal
#define TAG_LST  0x2  // Symbol literal
#define TAG_LNT  0x6  // Next N literals (110 binary)
#define TAG_EXT  0x7  // Extension (111 binary)

// Helper to create tagged cells
static inline uint64_t make_xt(void* addr) {
    return ((uint64_t)addr & ~0x3ULL) | TAG_XT;
}

static inline uint64_t make_exit(void) {
    return 0;  // EXIT is now XT(0)
}

static inline uint64_t make_lit(int64_t value) {
    // Embed 62-bit signed value in upper bits, tag=01
    return ((uint64_t)value << 2) | TAG_LIT;
}

static inline uint64_t make_lnt(uint64_t count) {
    // Embed count in upper 61 bits, tag=110
    return (count << 3) | TAG_LNT;
}

// Test 1: Simple arithmetic: 5 3 + (should leave 8)
void test_simple_add(void) {
    printf("\n=== Test 1: Simple Addition ===\n");
    printf("Program: 5 3 +\n");

    uint64_t program[] = {
        make_lit(5),        // Push 5 (single cell!)
        make_lit(3),        // Push 3 (single cell!)
        make_xt(op_add),    // Add them
        make_exit()         // EXIT (XT with addr=0)
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
        make_lit(10),       // Push 10 (single cell!)
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
        make_lit(7),        // Push 7 (single cell!)
        make_lit(3),        // Push 3 (single cell!)
        make_xt(op_sub),    // Subtract (4)
        make_lit(2),        // Push 2 (single cell!)
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
        make_lit(5),        // Push 5 (single cell!)
        make_lit(5),        // Push 5 (single cell!)
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
        make_lit(10),       // Push 10 (single cell!)
        make_lit(20),       // Push 20 (single cell!)
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

// Test 6: LNT (bulk literals): Push 1 2 3 4 5
void test_lnt_literals(void) {
    printf("\n=== Test 6: LNT Bulk Literals ===\n");
    printf("Program: [LNT:2] 10 20\n");
    printf("Expected: Stack should have 10 20\n");

    uint64_t program[] = {
        make_lnt(2),        // Next 2 cells are raw literals
        10, 20,             // Raw 64-bit values (no tags!)
        make_exit()
    };

    printf("Program array:\n");
    for (int i = 0; i < 4; i++) {
        printf("  [%d] = 0x%lx\n", i, program[i]);
    }
    fflush(stdout);

    vm_init();
    printf("Running VM...\n"); fflush(stdout);
    vm_run(program);
    printf("VM returned\n"); fflush(stdout);

    uint64_t* dsp = vm_get_dsp();

    // Stack grows downward, so check from top (dsp[0]) to bottom
    int passed = 1;
    uint64_t expected[] = {20, 10};  // Reversed because stack grows down

    printf("Stack contents: ");
    for (int i = 0; i < 2; i++) {
        printf("%lu ", dsp[i]);
        if (dsp[i] != expected[i]) {
            passed = 0;
        }
    }
    printf("\n");

    printf("Test %s\n", passed ? "PASSED" : "FAILED");
}

int main(void) {
    printf("March VM Test Suite\n");
    printf("===================\n");
    fflush(stdout);

    printf("Starting test 1...\n"); fflush(stdout);
    test_simple_add();

    printf("Starting test 2...\n"); fflush(stdout);
    test_dup_add();

    printf("Starting test 3...\n"); fflush(stdout);
    test_complex();

    printf("Starting test 4...\n"); fflush(stdout);
    test_comparison();

    printf("Starting test 5...\n"); fflush(stdout);
    test_swap();

    printf("Starting test 6...\n"); fflush(stdout);
    test_lnt_literals();

    printf("\n===================\n");
    printf("All tests complete!\n");

    return 0;
}
