#include <stdio.h>
#include "src/hamt.h"

int main() {
    printf("Testing 50 element insertion...\n");
    void* map = hamt_new();

    for (int i = 0; i < 50; i++) {
        printf("Inserting %d -> %d...\n", i, i * 10);
        fflush(stdout);
        map = hamt_set(map, i, i * 10);
        printf("  size = %lu\n", hamt_size(map));
    }

    printf("\nVerifying values...\n");
    for (int i = 0; i < 50; i++) {
        uint64_t val = hamt_get(map, i);
        printf("  get(%d) = %lu (expected %d)\n", i, val, i * 10);
    }

    printf("\nDone!\n");
    return 0;
}
