#include <stdio.h>
#include <stdint.h>
#include "src/hamt/hamt.h"

// Simple hash function for i64 keys
uint32_t hash_i64(const void *key, const size_t gen) {
    (void)gen; // Unused for now
    uint64_t k = (uint64_t)key;
    // Simple hash
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    return (uint32_t)k;
}

// Simple comparison function for i64 keys
int cmp_i64(const void *lhs, const void *rhs) {
    uint64_t l = (uint64_t)lhs;
    uint64_t r = (uint64_t)rhs;
    if (l < r) return -1;
    if (l > r) return 1;
    return 0;
}

int main() {
    printf("Testing libhamt with i64 keys...\n");

    // Configure HAMT
    struct hamt_config cfg = {
        .ator = &hamt_allocator_default,
        .key_cmp_fn = cmp_i64,
        .key_hash_fn = hash_i64,
    };

    struct hamt *map = hamt_create(&cfg);
    printf("Created map\n");

    // Insert values (cast i64 to void*)
    hamt_set(map, (void*)10, (void*)100);
    hamt_set(map, (void*)20, (void*)200);
    hamt_set(map, (void*)30, (void*)300);

    printf("Inserted 3 values\n");
    printf("Size: %zu\n", hamt_size(map));

    // Get values
    const void *v1 = hamt_get(map, (void*)10);
    const void *v2 = hamt_get(map, (void*)20);
    const void *v3 = hamt_get(map, (void*)30);

    printf("get(10) = %ld\n", (int64_t)v1);
    printf("get(20) = %ld\n", (int64_t)v2);
    printf("get(30) = %ld\n", (int64_t)v3);

    // Test with 100 elements
    printf("\nInserting 100 elements...\n");
    for (int i = 0; i < 100; i++) {
        hamt_set(map, (void*)(int64_t)i, (void*)(int64_t)(i * 10));
    }
    printf("Size: %zu\n", hamt_size(map));

    // Verify
    printf("Verifying values...\n");
    for (int i = 0; i < 100; i++) {
        const void *val = hamt_get(map, (void*)(int64_t)i);
        if ((int64_t)val != i * 10) {
            printf("ERROR: get(%d) = %ld, expected %d\n", i, (int64_t)val, i * 10);
            return 1;
        }
    }
    printf("All values correct!\n");

    hamt_delete(map);
    printf("\nTest passed!\n");

    return 0;
}
