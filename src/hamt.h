#ifndef HAMT_H
#define HAMT_H

#include <stdint.h>
#include <stddef.h>

/*
 * HAMT - Hash Array Mapped Trie
 *
 * Persistent hash map implementation for March language.
 * See docs/design/HAMT.md for full algorithm details.
 *
 * Key properties:
 * - O(log₃₂ n) operations (effectively O(1) in practice)
 * - Structural sharing via path copying
 * - Bitmap compression for memory efficiency
 * - i64 -> i64 mappings (values can be pointers)
 */

// HAMT node header (32 bytes, allocated before slots)
typedef struct {
    uint32_t bitmap;        // Occupancy bitmap (32 bits, one per possible slot)
    uint32_t count;         // Total number of elements in this subtree
    uint8_t  height;        // Distance from root (0 = leaf level)
    uint8_t  flags;         // Reserved for future use (collision nodes, etc.)
    uint8_t  padding[22];   // Reserved for alignment and future expansion
} hamt_header_t;

// Slot structure (16 bytes each)
typedef struct {
    uint64_t key;           // At leaf: actual key; at branch: key hash
    uint64_t value;         // At leaf: actual value; at branch: child pointer
} hamt_slot_t;

// Constants
#define HAMT_BITS_PER_LEVEL 5
#define HAMT_BRANCH_FACTOR  32    // 2^5
#define HAMT_LEVEL_MASK     0x1F  // 5 bits

// Pointer tagging for distinguishing leaf values from child pointers
// Since pointers are 8-byte aligned on x86-64, lowest 3 bits are always 0
// We use bit 0 to tag leaf values
#define HAMT_TAG_MASK       0x1   // Lowest bit
#define HAMT_TAG_VALUE      0x1   // Tagged = leaf value
#define HAMT_TAG_PTR        0x0   // Untagged = child pointer

// Tag/untag macros
#define hamt_tag_value(val)     ((void*)((uint64_t)(val) | HAMT_TAG_VALUE))
#define hamt_untag_value(val)   ((uint64_t)(val) & ~HAMT_TAG_MASK)
#define hamt_is_value(ptr)      (((uint64_t)(ptr) & HAMT_TAG_MASK) == HAMT_TAG_VALUE)
#define hamt_is_child(ptr)      (((uint64_t)(ptr) & HAMT_TAG_MASK) == HAMT_TAG_PTR)

/*
 * Core operations
 */

// Create new empty HAMT (returns NULL - lazy allocation on first insert)
void* hamt_new(void);

// Get value for key (returns 0 if key not found - caller must handle this)
// TODO: Consider returning a success flag via pointer parameter
uint64_t hamt_get(void* node, uint64_t key);

// Set key to value, returns new root (old root unchanged due to persistence)
// Allocates O(log n) new nodes via path copying
void* hamt_set(void* node, uint64_t key, uint64_t value);

// Remove key from map, returns new root (old root unchanged)
// Returns original node if key not found
void* hamt_remove(void* node, uint64_t key);

// Get total number of key-value pairs in map (O(1) - cached in root)
uint64_t hamt_size(void* node);

// Recursively free all nodes in HAMT tree
// WARNING: Does not free values if they are heap pointers!
void hamt_free(void* node);

/*
 * Utility functions
 */

// Hash a 64-bit key using FNV-1a algorithm
uint64_t hamt_hash(uint64_t key);

// Count number of set bits in bitmap (population count)
int hamt_popcount(uint32_t bitmap);

// Get array index for a bitmap bit position
// Uses popcount to map sparse bitmap to dense array
int hamt_slot_index(uint32_t bitmap, int bit);

/*
 * Internal helpers (exposed for testing/debugging)
 */

// Extract 5-bit chunk from hash at given level (0 = root)
static inline int hamt_chunk(uint64_t hash, int level) {
    return (hash >> (level * HAMT_BITS_PER_LEVEL)) & HAMT_LEVEL_MASK;
}

// Get pointer to node's header (32 bytes before slot array)
static inline hamt_header_t* hamt_get_header(void* node) {
    if (node == NULL) return NULL;
    return (hamt_header_t*)((char*)node - sizeof(hamt_header_t));
}

// Get pointer to node's slot array (node pointer points here)
static inline hamt_slot_t* hamt_get_slots(void* node) {
    return (hamt_slot_t*)node;
}

#endif // HAMT_H
