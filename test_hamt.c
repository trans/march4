#include <stdio.h>
#include <assert.h>
#include "src/hamt.h"

void test_basic_operations() {
    printf("Testing basic HAMT operations...\n");

    // Create empty map
    void* map = hamt_new();
    assert(map == NULL);  // Lazy allocation
    assert(hamt_size(map) == 0);

    // Insert some values
    map = hamt_set(map, 10, 100);
    assert(hamt_size(map) == 1);
    assert(hamt_get(map, 10) == 100);

    map = hamt_set(map, 20, 200);
    assert(hamt_size(map) == 2);
    assert(hamt_get(map, 10) == 100);
    assert(hamt_get(map, 20) == 200);

    map = hamt_set(map, 30, 300);
    assert(hamt_size(map) == 3);
    assert(hamt_get(map, 30) == 300);

    // Update existing value
    map = hamt_set(map, 20, 222);
    assert(hamt_size(map) == 3);  // Size shouldn't change
    assert(hamt_get(map, 20) == 222);

    // Remove a value
    map = hamt_remove(map, 20);
    assert(hamt_size(map) == 2);
    assert(hamt_get(map, 20) == 0);  // Key not found
    assert(hamt_get(map, 10) == 100);  // Others still there
    assert(hamt_get(map, 30) == 300);

    // Clean up
    hamt_free(map);

    printf("✓ Basic operations passed\n");
}

void test_persistence() {
    printf("Testing persistence (structural sharing)...\n");

    void* map1 = hamt_new();
    map1 = hamt_set(map1, 10, 100);
    map1 = hamt_set(map1, 20, 200);

    // Create new version by adding to map1
    void* map2 = hamt_set(map1, 30, 300);

    // Both versions should exist independently
    assert(hamt_size(map1) == 2);
    assert(hamt_size(map2) == 3);

    assert(hamt_get(map1, 10) == 100);
    assert(hamt_get(map1, 20) == 200);
    assert(hamt_get(map1, 30) == 0);  // Not in map1

    assert(hamt_get(map2, 10) == 100);
    assert(hamt_get(map2, 20) == 200);
    assert(hamt_get(map2, 30) == 300);  // In map2

    // Clean up both versions
    hamt_free(map1);
    hamt_free(map2);

    printf("✓ Persistence test passed\n");
}

void test_larger_map() {
    printf("Testing larger map (100 elements)...\n");

    void* map = hamt_new();

    // Insert 100 elements
    for (int i = 0; i < 100; i++) {
        map = hamt_set(map, i, i * 10);
    }

    assert(hamt_size(map) == 100);

    // Verify all elements
    for (int i = 0; i < 100; i++) {
        assert(hamt_get(map, i) == i * 10);
    }

    // Remove half of them
    for (int i = 0; i < 50; i++) {
        map = hamt_remove(map, i);
    }

    assert(hamt_size(map) == 50);

    // Verify remaining elements
    for (int i = 50; i < 100; i++) {
        assert(hamt_get(map, i) == i * 10);
    }

    // Verify removed elements are gone
    for (int i = 0; i < 50; i++) {
        assert(hamt_get(map, i) == 0);
    }

    hamt_free(map);

    printf("✓ Larger map test passed\n");
}

void test_collision_handling() {
    printf("Testing collision handling...\n");

    void* map = hamt_new();

    // These might collide depending on the hash function
    // We'll just test that they can coexist
    map = hamt_set(map, 1, 10);
    map = hamt_set(map, 2, 20);
    map = hamt_set(map, 3, 30);

    assert(hamt_get(map, 1) == 10);
    assert(hamt_get(map, 2) == 20);
    assert(hamt_get(map, 3) == 30);

    hamt_free(map);

    printf("✓ Collision test passed\n");
}

int main() {
    printf("=== HAMT Test Suite ===\n\n");

    printf("Starting test_basic_operations...\n");
    fflush(stdout);
    test_basic_operations();

    printf("\nStarting test_persistence...\n");
    fflush(stdout);
    test_persistence();

    printf("\nStarting test_larger_map...\n");
    fflush(stdout);
    test_larger_map();

    printf("\nStarting test_collision_handling...\n");
    fflush(stdout);
    test_collision_handling();

    printf("\n=== All tests passed! ===\n");

    return 0;
}
