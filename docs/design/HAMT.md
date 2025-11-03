# HAMT - Hash Array Mapped Trie

**Status:** ✅ Implemented
**Date:** 2025-11-02
**Purpose:** Persistent hash map implementation for March language
**Implementation:** `src/hamt.h`, `src/hamt.c`, `kernel/x86-64/map-*.asm`

---

## Overview

HAMT (Hash Array Mapped Trie) provides efficient persistent hash maps with:
- **O(log₃₂ n)** lookup, insert, and remove operations
- **Structural sharing** - modifications create minimal new nodes
- **Path copying** - immutability via copy-on-write along modification path
- **Compact storage** - bitmap compression eliminates empty slots

**References:**
- Phil Bagwell (2000): "Ideal Hash Trees"
- Clojure's PersistentHashMap implementation
- Rich Hickey's design notes
- [mkirchner/hamt](https://github.com/mkirchner/hamt) (MIT) - Reference implementation that taught us pointer tagging

---

## Algorithm Basics

### Trie Structure

Each HAMT node is a 32-way branching trie:
- Hash divided into 5-bit chunks (2^5 = 32 possible branches)
- Each chunk indexes into current node's slot array
- Depth in tree = which 5-bit chunk we're examining

**Example hash traversal:**
```
Hash: 0xABCDEF12
Level 0: bits [0-4]   = 0x12 & 0x1F = 0x12 (chunk 1)
Level 1: bits [5-9]   = 0x12 >> 5   = 0x00 (chunk 2)
Level 2: bits [10-14] = 0xEF >> 2   = 0x3B (chunk 3)
...and so on
```

### Bitmap Compression

Instead of allocating 32 slots per node, we use:
1. **32-bit bitmap** - bit N set means slot N is occupied
2. **Compressed array** - only occupied slots stored
3. **Mapping function** - `popcount(bitmap & ((1 << index) - 1))` gives array position

**Example:**
```
Bitmap: 0x00000141 (bits 0, 6, 8 set)
Slots:  [slot₀, slot₆, slot₈]  (only 3 slots allocated)

To access index 6:
  mask = (1 << 6) - 1 = 0x3F
  popcount(0x141 & 0x3F) = popcount(0x01) = 1
  → slot₆ is at array[1]
```

### Path Copying

On modification:
1. Navigate to target node
2. Create new leaf with updated value
3. Clone each node on path back to root
4. Return new root (old tree unchanged)

**Memory sharing:**
- Unmodified subtrees shared between old and new versions
- Only O(log n) nodes cloned per modification
- Garbage collection via refcounting or manual FREE

---

## Memory Layout

### Node Header (32 bytes)

Aligned with March's array/string header pattern:

```c
typedef struct {
    uint32_t bitmap;        // Occupancy bitmap (32 bits)
    uint32_t count;         // Total elements in subtree
    uint8_t  height;        // Distance from root (0 = leaf)
    uint8_t  flags;         // Future: collision nodes, etc.
    uint8_t  padding[22];   // Reserved for future use
} hamt_header_t;
```

**Header location:** 32 bytes before first slot
**Total node size:** 32 + (popcount(bitmap) * 16)
- Each slot = 16 bytes (8-byte key + 8-byte value for i64 types)

### Slot Types and Pointer Tagging

**Critical Design Decision:** Each slot can independently be a leaf OR a branch, determined by **pointer tagging**.

Since pointers on x86-64 are 8-byte aligned, the lowest 3 bits are always 0. We use bit 0 to tag values:

```c
#define HAMT_TAG_MASK   0x1   // Lowest bit
#define HAMT_TAG_VALUE  0x1   // Tagged = leaf value
#define HAMT_TAG_PTR    0x0   // Untagged = child pointer
```

**Leaf slot (tagged value):**
```
[key: u64][value: u64 | TAG_VALUE]
         ^-- bit 0 set to 1
```

**Branch slot (untagged pointer):**
```
[key: unused][child_ptr: u64]
              ^-- bit 0 is 0 (aligned pointer)
```

**Why Pointer Tagging?**

The initial implementation attempted to use a node-level `height` or `flags` field to distinguish all slots in a node as either leaves or branches. This failed because:

- A single node can have SOME slots as leaves and SOME as branches
- When a collision occurs at one slot, we convert just that slot to point to a child
- Other slots in the same node remain as leaves
- Node-level flags caused segfaults when trying to dereference leaf values as pointers

**Pointer tagging solves this:** Each slot is self-describing and independent.

### Collision Handling

When two different keys hash to the same position:

1. **Current slot is a leaf** (tagged value) with key₁
2. **New key₂ needs same position**
3. **Create child node** at next level containing both keys
4. **Replace leaf slot** with branch slot (untagged child pointer)
5. **Recursively insert** both keys into child node

This naturally handles any number of collisions by pushing them down the trie until the hash chunks differ.

---

## Hash Function

**Choice: FNV-1a (Fowler-Noll-Vo)**

Simple, fast, good distribution:

```c
uint64_t fnv1a_hash(uint64_t key) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    uint8_t* bytes = (uint8_t*)&key;

    for (int i = 0; i < 8; i++) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;  // FNV prime
    }
    return hash;
}
```

**Properties:**
- Fast (2-3 cycles per byte on modern CPUs)
- Good avalanche (single bit change affects entire hash)
- No cryptographic strength needed (not a security primitive)

**Future:** Support custom hash functions for string keys, composite keys, etc.

---

## Core Operations

### 1. GET - Lookup Value

```
hamt_get(node_ptr, key) -> value | NULL
```

**Algorithm:**
1. Hash the key → `hash`
2. Extract 5-bit chunk for current level
3. Check if `bitmap` has bit set for chunk
4. If not set → key not found, return NULL
5. If set → compute slot index via popcount
6. If leaf (height=0):
   - Check if slot's key matches → return value
   - Else collision → linear search collision list
7. If branch (height>0):
   - Recurse into child node

**Assembly primitive:** `march.map.get` (map key -> value)

### 2. SET - Insert/Update

```
hamt_set(node_ptr, key, value) -> new_node_ptr
```

**Algorithm:**
1. Hash key → `hash`
2. Navigate to target position (same as GET)
3. **If key exists:** Clone node, update value, return new node
4. **If key doesn't exist:**
   - Clone node
   - Set bitmap bit for new slot
   - Allocate expanded slot array
   - Insert new key/value
   - Return new node
5. **Path copying:** Clone all ancestors back to root

**Assembly primitive:** `march.map.set` (map key value -> map)

### 3. REMOVE - Delete Key

```
hamt_remove(node_ptr, key) -> new_node_ptr
```

**Algorithm:**
1. Navigate to key position
2. If not found → return original node (no change)
3. If found:
   - Clone node
   - Clear bitmap bit
   - Allocate smaller slot array (omit deleted slot)
   - Return new node
4. **Optimization:** If removal leaves node with 1 child, collapse upward

**Assembly primitive:** `march.map.remove` (map key -> map)

---

## Integration with March

### Type System

New type: `TYPE_MAP` (immutable hash map)

```march
Type signature:
  march.map.new    -> map
  march.map.get    map i64 -> i64
  march.map.set    map i64 i64 -> map
  march.map.remove map i64 -> map
  march.map.size   map -> i64
```

No mutable variant initially (persistent = always immutable).

### Memory Management

**Allocation:**
- Use existing `malloc` via ALLOC primitive
- Each node allocated independently

**Deallocation:**
- Manual: Implement `march.map.free` that recursively frees subtrees
- Future: Integrate with slot-based tracking (reference counting)

**Path copying allocation:**
- SET operation allocates O(log n) new nodes
- Old nodes remain valid until freed
- Enables efficient undo/versioning

### Example Usage

```march
( Create empty map )
march.map.new

( Insert values - each returns new map )
10 100 march.map.set  ( key=10, value=100 )
20 200 march.map.set  ( key=20, value=200 )
30 300 march.map.set  ( key=30, value=300 )

( Lookup )
dup 20 march.map.get  ( Returns 200 )

( Remove )
30 march.map.remove   ( Returns new map without key 30 )
```

---

## Implementation Plan

### Phase 1: Core HAMT in C

**Files to create:**
- `src/hamt.h` - Data structures, function declarations
- `src/hamt.c` - Core algorithm implementation

**Functions:**
```c
// Core operations
void*    hamt_new();
uint64_t hamt_get(void* node, uint64_t key);
void*    hamt_set(void* node, uint64_t key, uint64_t value);
void*    hamt_remove(void* node, uint64_t key);
uint64_t hamt_size(void* node);

// Utilities
uint64_t hamt_hash(uint64_t key);        // FNV-1a
int      hamt_popcount(uint32_t bitmap); // Count set bits
int      hamt_slot_index(uint32_t bitmap, int bit);
void     hamt_free(void* node);          // Recursive free
```

### Phase 2: Assembly Primitives

**Files to create:**
- `kernel/x86-64/map-new.asm`
- `kernel/x86-64/map-get.asm`
- `kernel/x86-64/map-set.asm`
- `kernel/x86-64/map-remove.asm`
- `kernel/x86-64/map-size.asm`

Each primitive:
1. Pops arguments from data stack
2. Calls C function (saves/restores callee-saved registers)
3. Pushes result to data stack
4. Returns to VM dispatch loop

### Phase 3: Testing

**Test files:**
- `test_map_basic.march` - Create, insert, lookup
- `test_map_chain.march` - Multiple operations, verify persistence
- `test_map_stress.march` - Large maps, collision handling

**C unit tests:**
- `tests/test_hamt.c` - Direct C function testing
- Verify bitmap compression
- Verify path copying
- Verify collision handling

### Phase 4: Optimization

**After basic implementation works:**
- Profile with `perf` to find hotspots
- Optimize popcount (use `__builtin_popcount` or `popcnt` instruction)
- Optimize slot_index calculation
- Consider node pooling for allocation

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| GET       | O(log₃₂ n) | Max depth ≈ 13 for 64-bit hashes |
| SET       | O(log₃₂ n) | Includes path copying |
| REMOVE    | O(log₃₂ n) | Includes path copying |
| SIZE      | O(1)       | Stored in root header |

### Space Complexity

**Per node overhead:**
- Header: 32 bytes
- Slot: 16 bytes (key + value)
- Average occupancy: ~16 slots per node (50% full)
- Average node size: 32 + 16*16 = 288 bytes

**Total space for N elements:**
- Nodes: N / 16 nodes × 288 bytes ≈ 18N bytes
- Comparison to naive array: 16N bytes (key+value pairs)
- Overhead: ~12% (excellent for persistent structure!)

### Practical Performance

**Expected real-world:**
- Small maps (≤100 keys): 2-3 node traversals average
- Medium maps (≤10K keys): 3-4 node traversals
- Large maps (≤1M keys): 4-5 node traversals

**Memory sharing benefits:**
- Two maps differing by 1 element share 99%+ of nodes
- Enables efficient versioning, undo, speculative updates

---

## Future Enhancements

### 1. Transient Operations (Clojure-style)

For bulk updates, allow in-place mutation:
```march
march.map.transient  map -> map!           ( Convert to mutable )
march.map.set!       map! key val -> map!  ( In-place update )
march.map.persistent map! -> map           ( Convert back to immutable )
```

Benefits: Batch updates 10-100× faster.

### 2. Custom Key Types

Support string keys, composite keys:
```march
march.map.set/str  map str i64 -> map  ( String keys )
march.map.set/pair map i64 i64 i64 -> map  ( Composite keys )
```

### 3. Iteration

Iterator support:
```march
march.map.iter      map -> iter
march.iter.next     iter -> key value iter | null
march.iter.foreach  iter quot -> ( )  ( Apply quotation to each pair )
```

### 4. Advanced Collision Handling

Current design uses linear lists. Future options:
- Tree collision nodes (rare, but handles pathological cases)
- Secondary hash function
- Adaptive collision strategy

---

## Implementation Decisions

These questions were resolved during implementation:

1. **Should SIZE be cached in every node, or only root?** ✅
   - **Implemented:** Cache in all nodes
   - Size updated during path copying (already cloning ancestors)
   - Enables O(1) `march.map.size` operation

2. **Initial map allocation - empty root or NULL?** ✅
   - **Implemented:** Use NULL (lazy allocation)
   - `march.map.new` returns NULL
   - First `march.map.set` creates root node
   - Saves allocation for empty/unused maps

3. **Reference counting or manual free?** ✅
   - **Implemented:** Manual free via `march.map.free`
   - Simple, explicit memory management
   - User responsible for calling free when done
   - Future: Add reference counting or GC integration

4. **How to distinguish leaf slots from branch slots?** ✅ (Critical!)
   - **Implemented:** Pointer tagging (not node-level flags)
   - Bit 0 of value field: 1 = leaf value, 0 = child pointer
   - Each slot is self-describing and independent
   - See "Slot Types and Pointer Tagging" section above

5. **Should we expose HAMT internals for debugging?**
   - **Not yet implemented**
   - Future: `march.map.debug.dump`, `march.map.debug.stats`
   - Would help with performance tuning and understanding structure

---

## Summary

HAMT provides efficient, persistent hash maps for March:
- ✅ Clean persistent semantics (immutable by default)
- ✅ Excellent performance (O(log₃₂ n) ≈ O(1) in practice)
- ✅ Structural sharing enables cheap versioning
- ✅ Pointer tagging for efficient leaf/branch distinction
- ✅ Foundation for future persistent data structures
- ✅ Aligns with March's memory model and type system

**Implementation Status:** ✅ Complete

**Files:**
- `src/hamt.h` - Public API and data structures
- `src/hamt.c` - Core HAMT implementation (~450 lines)
- `kernel/x86-64/map-*.asm` - Assembly primitives (6 operations)
- `src/hamt/` - Reference libhamt library (MIT licensed, kept for future use)

**Test Coverage:**
- ✅ Basic operations (new, get, set, remove, size, free)
- ✅ 100+ element maps
- ✅ Collision handling
- ✅ Persistence (structural sharing)
- ✅ Memory management

**Next Steps:**
1. Register primitives in compiler/runtime vocabulary
2. Test with actual March programs
3. Implement symbol table (`src/symtbl.c`) for string key interning
4. Add convenience operations (contains?, keys, values, etc.)
5. Consider optimization (popcount intrinsic, node pooling)
6. Move on to RRB-trees for persistent vectors

**Lessons Learned:**
- Pointer tagging is essential for HAMTs - don't use node-level flags
- Reference libhamt implementation taught us the right approach
- FNV-1a hash is simple and effective for i64 keys
- Lazy allocation (NULL for empty maps) works well
