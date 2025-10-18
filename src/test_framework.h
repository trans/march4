/*
 * Simple Test Framework
 * Minimal assertion-based testing
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test counters */
static int _test_total = 0;
static int _test_passed = 0;
static int _test_failed = 0;

/* Color codes */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RESET   "\033[0m"

/* Assert macros */
#define ASSERT(expr) do { \
    _test_total++; \
    if (expr) { \
        _test_passed++; \
        printf(COLOR_GREEN "  ✓" COLOR_RESET " %s\n", #expr); \
    } else { \
        _test_failed++; \
        printf(COLOR_RED "  ✗" COLOR_RESET " %s (line %d)\n", #expr, __LINE__); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    _test_total++; \
    if ((a) == (b)) { \
        _test_passed++; \
        printf(COLOR_GREEN "  ✓" COLOR_RESET " %s == %s\n", #a, #b); \
    } else { \
        _test_failed++; \
        printf(COLOR_RED "  ✗" COLOR_RESET " %s == %s (got %lld vs %lld, line %d)\n", \
               #a, #b, (long long)(a), (long long)(b), __LINE__); \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    _test_total++; \
    if (strcmp((a), (b)) == 0) { \
        _test_passed++; \
        printf(COLOR_GREEN "  ✓" COLOR_RESET " %s == \"%s\"\n", #a, (b)); \
    } else { \
        _test_failed++; \
        printf(COLOR_RED "  ✗" COLOR_RESET " %s == \"%s\" (got \"%s\", line %d)\n", \
               #a, (b), (a), __LINE__); \
    } \
} while(0)

#define ASSERT_NULL(ptr) ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)

/* Test suite macros */
#define TEST_SUITE(name) \
    printf("\n" COLOR_YELLOW "=== " name " ===" COLOR_RESET "\n")

#define TEST_SUMMARY() do { \
    printf("\n" COLOR_YELLOW "==================" COLOR_RESET "\n"); \
    printf("Tests: %d total, ", _test_total); \
    printf(COLOR_GREEN "%d passed" COLOR_RESET ", ", _test_passed); \
    if (_test_failed > 0) { \
        printf(COLOR_RED "%d failed" COLOR_RESET "\n", _test_failed); \
    } else { \
        printf("%d failed\n", _test_failed); \
    } \
    return _test_failed > 0 ? 1 : 0; \
} while(0)

#endif /* TEST_FRAMEWORK_H */
