#include <stdio.h>
#include "src/hamt.h"

int main() {
    printf("Creating empty map...\n");
    void* map = hamt_new();
    printf("map = %p\n", map);

    printf("\nInserting key=10, value=100...\n");
    map = hamt_set(map, 10, 100);
    printf("map = %p\n", map);
    printf("size = %lu\n", hamt_size(map));

    printf("\nGetting key=10...\n");
    uint64_t val = hamt_get(map, 10);
    printf("value = %lu\n", val);

    printf("\nInserting key=20, value=200...\n");
    map = hamt_set(map, 20, 200);
    printf("map = %p\n", map);
    printf("size = %lu\n", hamt_size(map));

    printf("\nGetting key=20...\n");
    val = hamt_get(map, 20);
    printf("value = %lu\n", val);

    printf("\nDone!\n");
    return 0;
}
