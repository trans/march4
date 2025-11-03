#include "hamt.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
 * HAMT Implementation
 * See docs/design/HAMT.md for algorithm details
 */

// ============================================================================
// Utility Functions
// ============================================================================

// FNV-1a hash function
uint64_t hamt_hash(uint64_t key) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    uint8_t* bytes = (uint8_t*)&key;

    for (int i = 0; i < 8; i++) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;  // FNV prime
    }

    return hash;
}

// Population count (number of set bits in bitmap)
// TODO: Use __builtin_popcount or popcnt instruction for optimization
int hamt_popcount(uint32_t bitmap) {
    int count = 0;
    while (bitmap) {
        count += bitmap & 1;
        bitmap >>= 1;
    }
    return count;
}

// Get array index for a bit position using popcount
int hamt_slot_index(uint32_t bitmap, int bit) {
    // Mask off all bits above the target bit, then count remaining bits
    uint32_t mask = (1U << bit) - 1;
    return hamt_popcount(bitmap & mask);
}

// ============================================================================
// Node Allocation
// ============================================================================

// Allocate a new node with specified bitmap and copy slots
// Returns pointer to slot array (header is 32 bytes before)
static void* hamt_alloc_node(uint32_t bitmap, uint32_t count,
                             hamt_slot_t* slots, int num_slots) {
    int slot_count = hamt_popcount(bitmap);
    size_t total_size = sizeof(hamt_header_t) + (slot_count * sizeof(hamt_slot_t));

    // Allocate header + slots
    hamt_header_t* header = (hamt_header_t*)malloc(total_size);
    if (!header) return NULL;

    // Initialize header
    header->bitmap = bitmap;
    header->count = count;
    header->height = 0;   // Unused with pointer tagging
    header->flags = 0;    // Unused with pointer tagging
    memset(header->padding, 0, sizeof(header->padding));

    // Get pointer to slot array (after header)
    hamt_slot_t* node_slots = (hamt_slot_t*)(header + 1);

    // Copy slots if provided
    if (slots && num_slots > 0) {
        memcpy(node_slots, slots, num_slots * sizeof(hamt_slot_t));
    }

    // Return pointer to slot array (this is the "node" pointer)
    return node_slots;
}

// Clone a node (for path copying)
static void* hamt_clone_node(void* node) {
    if (!node) return NULL;

    hamt_header_t* header = hamt_get_header(node);
    hamt_slot_t* slots = hamt_get_slots(node);
    int slot_count = hamt_popcount(header->bitmap);

    return hamt_alloc_node(header->bitmap, header->count, slots, slot_count);
}

// ============================================================================
// Core Operations
// ============================================================================

void* hamt_new(void) {
    trace_push("hamt_new()");
    // Lazy allocation - return NULL, first insert will create root
    return NULL;
}

uint64_t hamt_size(void* node) {
    trace_push_value((uint64_t)node, "hamt_size(node=%p)", node);
    if (!node) {
        trace_pop(); // hamt_size
        return 0;
    }
    hamt_header_t* header = hamt_get_header(node);
    uint64_t size = header->count;
    trace_push_value(size, "hamt_size: returning %lu", size);
    trace_pop(); // returning
    trace_pop(); // hamt_size
    return size;
}

// ============================================================================
// GET Operation
// ============================================================================

uint64_t hamt_get(void* node, uint64_t key) {
    trace_push_value(key, "hamt_get(node=%p, key=%lu)", node, key);

    if (!node) {
        trace_push("hamt_get: empty map, returning 0");
        trace_pop(); // hamt_get: empty map
        trace_pop(); // hamt_get
        return 0;  // Empty map
    }

    uint64_t hash = hamt_hash(key);
    int level = 0;

    while (node) {
        hamt_header_t* header = hamt_get_header(node);
        hamt_slot_t* slots = hamt_get_slots(node);

        // Extract 5-bit chunk for this level
        int chunk = hamt_chunk(hash, level);
        uint32_t bit = 1U << chunk;

        // Check if slot exists
        if (!(header->bitmap & bit)) {
            return 0;  // Key not found
        }

        // Get slot index and access slot
        int idx = hamt_slot_index(header->bitmap, chunk);
        hamt_slot_t* slot = &slots[idx];

        // Check if this slot contains a value (leaf) or child pointer (branch)
        // Using pointer tagging: tagged = value, untagged = child pointer
        if (hamt_is_value((void*)slot->value)) {
            // This slot is a leaf - compare keys
            if (slot->key == key) {
                uint64_t result = hamt_untag_value((void*)slot->value);
                trace_push_value(result, "hamt_get: found value=%lu", result);
                trace_pop(); // found value
                trace_pop(); // hamt_get
                return result;
            } else {
                // Key mismatch at leaf
                trace_push("hamt_get: key mismatch at leaf, returning 0");
                trace_pop(); // key mismatch
                trace_pop(); // hamt_get
                return 0;
            }
        }

        // This slot is a branch - descend to child
        trace_push_value(level, "hamt_get: descending to level %d", level + 1);
        trace_pop(); // descending
        node = (void*)slot->value;
        level++;
    }

    trace_push("hamt_get: not found, returning 0");
    trace_pop(); // not found
    trace_pop(); // hamt_get
    return 0;  // Not found
}

// ============================================================================
// SET Operation
// ============================================================================

// Forward declaration for recursion
static void* hamt_set_impl(void* node, uint64_t key, uint64_t value,
                          uint64_t hash, int level, bool* inserted);

void* hamt_set(void* node, uint64_t key, uint64_t value) {
    trace_push_value(key, "hamt_set(node=%p, key=%lu, value=%lu)", node, key, value);

    bool inserted = false;
    uint64_t hash = hamt_hash(key);
    void* result = hamt_set_impl(node, key, value, hash, 0, &inserted);
    trace_push_value((uint64_t)result, "hamt_set: returning node=%p", result);
    trace_pop(); // returning
    trace_pop(); // hamt_set
    return result;
}

static void* hamt_set_impl(void* node, uint64_t key, uint64_t value,
                          uint64_t hash, int level, bool* inserted) {
    // Base case: create new leaf node
    if (!node) {
        int chunk = hamt_chunk(hash, level);
        uint32_t bitmap = 1U << chunk;

        // Tag the value to mark this as a leaf slot
        hamt_slot_t slot = {
            .key = key,
            .value = (uint64_t)hamt_tag_value((void*)value)
        };
        *inserted = true;

        return hamt_alloc_node(bitmap, 1, &slot, 1);
    }

    hamt_header_t* header = hamt_get_header(node);
    hamt_slot_t* slots = hamt_get_slots(node);
    int chunk = hamt_chunk(hash, level);
    uint32_t bit = 1U << chunk;

    // Check if slot exists
    bool slot_exists = (header->bitmap & bit) != 0;
    int idx = slot_exists ? hamt_slot_index(header->bitmap, chunk) : 0;

    if (!slot_exists) {
        // Insert new slot
        int old_count = hamt_popcount(header->bitmap);
        int new_idx = hamt_slot_index(header->bitmap, chunk);

        // Allocate new slots array with room for one more
        hamt_slot_t* new_slots = malloc((old_count + 1) * sizeof(hamt_slot_t));

        // Copy slots before insertion point
        if (new_idx > 0) {
            memcpy(new_slots, slots, new_idx * sizeof(hamt_slot_t));
        }

        // Insert new slot as a leaf (tagged value)
        new_slots[new_idx].key = key;
        new_slots[new_idx].value = (uint64_t)hamt_tag_value((void*)value);

        // Copy slots after insertion point
        if (new_idx < old_count) {
            memcpy(&new_slots[new_idx + 1], &slots[new_idx],
                   (old_count - new_idx) * sizeof(hamt_slot_t));
        }

        // Create new node with updated bitmap and count
        uint32_t new_bitmap = header->bitmap | bit;
        uint32_t new_count = header->count + 1;

        void* result = hamt_alloc_node(new_bitmap, new_count, new_slots, old_count + 1);
        free(new_slots);
        *inserted = true;

        return result;
    }

    // Slot exists - check if it's a leaf or branch using pointer tagging
    hamt_slot_t* slot = &slots[idx];

    if (hamt_is_value((void*)slot->value)) {
        // This slot is a leaf
        if (slot->key == key) {
            // Update existing key - clone node and update value
            void* new_node = hamt_clone_node(node);
            hamt_slot_t* new_slots = hamt_get_slots(new_node);
            new_slots[idx].value = (uint64_t)hamt_tag_value((void*)value);
            return new_node;
        } else {
            // Collision - two different keys at same position
            // Push both keys down to next level in a new child node
            uint64_t old_key = slot->key;
            uint64_t old_value = hamt_untag_value((void*)slot->value);
            uint64_t old_hash = hamt_hash(old_key);

            // Create child node with both keys
            void* child = hamt_set_impl(NULL, old_key, old_value, old_hash, level + 1, inserted);
            child = hamt_set_impl(child, key, value, hash, level + 1, inserted);

            // Clone current node and convert this slot to point to child (untagged)
            void* new_node = hamt_clone_node(node);
            hamt_header_t* new_header = hamt_get_header(new_node);
            hamt_slot_t* new_slots = hamt_get_slots(new_node);

            new_slots[idx].key = 0;  // Branch slots don't use key field
            new_slots[idx].value = (uint64_t)child;  // Untagged = child pointer
            new_header->count = header->count + 1;

            return new_node;
        }
    } else {
        // This slot is a branch - recurse into child
        void* child = (void*)slot->value;
        void* new_child = hamt_set_impl(child, key, value, hash, level + 1, inserted);

        if (new_child == child) {
            return node;  // No change
        }

        // Clone node and update child pointer
        void* new_node = hamt_clone_node(node);
        hamt_header_t* new_header = hamt_get_header(new_node);
        hamt_slot_t* new_slots = hamt_get_slots(new_node);

        new_slots[idx].value = (uint64_t)new_child;  // Untagged = child pointer
        if (*inserted) {
            new_header->count = header->count + 1;
        }

        return new_node;
    }
}

// ============================================================================
// REMOVE Operation
// ============================================================================

// Forward declaration for recursion
static void* hamt_remove_impl(void* node, uint64_t key, uint64_t hash,
                             int level, bool* removed);

void* hamt_remove(void* node, uint64_t key) {
    if (!node) return NULL;

    bool removed = false;
    uint64_t hash = hamt_hash(key);
    return hamt_remove_impl(node, key, hash, 0, &removed);
}

static void* hamt_remove_impl(void* node, uint64_t key, uint64_t hash,
                             int level, bool* removed) {
    if (!node) return NULL;

    hamt_header_t* header = hamt_get_header(node);
    hamt_slot_t* slots = hamt_get_slots(node);
    int chunk = hamt_chunk(hash, level);
    uint32_t bit = 1U << chunk;

    // Check if slot exists
    if (!(header->bitmap & bit)) {
        return node;  // Key not found, no change
    }

    int idx = hamt_slot_index(header->bitmap, chunk);
    hamt_slot_t* slot = &slots[idx];

    if (hamt_is_value((void*)slot->value)) {
        // This slot is a leaf
        if (slot->key != key) {
            return node;  // Key not found
        }

        // Remove this leaf slot
        *removed = true;
        int old_count = hamt_popcount(header->bitmap);

        if (old_count == 1) {
            // Last slot - return NULL (empty node)
            return NULL;
        }

        // Allocate new slots without this one
        hamt_slot_t* new_slots = malloc((old_count - 1) * sizeof(hamt_slot_t));

        // Copy slots before removed slot
        if (idx > 0) {
            memcpy(new_slots, slots, idx * sizeof(hamt_slot_t));
        }

        // Copy slots after removed slot
        if (idx < old_count - 1) {
            memcpy(&new_slots[idx], &slots[idx + 1],
                   (old_count - idx - 1) * sizeof(hamt_slot_t));
        }

        uint32_t new_bitmap = header->bitmap & ~bit;
        uint32_t new_count = header->count - 1;

        void* result = hamt_alloc_node(new_bitmap, new_count, new_slots, old_count - 1);
        free(new_slots);

        return result;
    } else {
        // This slot is a branch - recurse into child
        void* child = (void*)slot->value;
        void* new_child = hamt_remove_impl(child, key, hash, level + 1, removed);

        if (new_child == child) {
            return node;  // No change
        }

        if (new_child == NULL) {
            // Child became empty - remove this slot
            int old_count = hamt_popcount(header->bitmap);

            if (old_count == 1) {
                // Last slot - this node becomes empty
                return NULL;
            }

            // Remove slot from this node
            hamt_slot_t* new_slots = malloc((old_count - 1) * sizeof(hamt_slot_t));

            if (idx > 0) {
                memcpy(new_slots, slots, idx * sizeof(hamt_slot_t));
            }
            if (idx < old_count - 1) {
                memcpy(&new_slots[idx], &slots[idx + 1],
                       (old_count - idx - 1) * sizeof(hamt_slot_t));
            }

            uint32_t new_bitmap = header->bitmap & ~bit;
            uint32_t new_count = header->count - 1;

            void* result = hamt_alloc_node(new_bitmap, new_count, new_slots, old_count - 1);
            free(new_slots);

            return result;
        }

        // Clone node and update child pointer
        void* new_node = hamt_clone_node(node);
        hamt_header_t* new_header = hamt_get_header(new_node);
        hamt_slot_t* new_slots = hamt_get_slots(new_node);

        new_slots[idx].value = (uint64_t)new_child;
        if (*removed) {
            new_header->count = header->count - 1;
        }

        return new_node;
    }
}

// ============================================================================
// FREE Operation
// ============================================================================

void hamt_free(void* node) {
    if (!node) return;

    hamt_header_t* header = hamt_get_header(node);
    hamt_slot_t* slots = hamt_get_slots(node);
    int slot_count = hamt_popcount(header->bitmap);

    // Check each slot - if it's a branch (untagged), recursively free it
    for (int i = 0; i < slot_count; i++) {
        if (hamt_is_child((void*)slots[i].value)) {
            // This slot contains a child pointer - recursively free it
            void* child = (void*)slots[i].value;
            hamt_free(child);
        }
        // If it's a tagged value (leaf), nothing to free
    }

    // Free this node (header is 32 bytes before slots)
    free(header);
}
