## HAMT Plan - Implementation Roadmap

## Progress

- ✅ Design HAMT memory layout (header + bitmap + slots)
- ✅ Implement HAMT core operations (get, set, remove, size, free)
- ✅ Create assembly primitives (march.map.new, get, set, remove, size, free)
- ✅ Integrate HAMT into build system
- ✅ Add pointer tagging for leaf/branch distinction
- ☐ Design RRB-tree memory layout for persistent vectors
- ☐ Implement RRB-tree array operations
- ☐ Add march.array.* primitives using RRB-trees (immutable by default)

## Phase 1: HAMT (Hash Array Mapped Trie) - Persistent Hash Maps

Why start here:
- Simpler than RRB-trees
- Well-documented algorithm (Bagwell, 2000)
- Foundation for future hash map/dict types

Memory layout:
HAMT Node Header (32 bytes):
  [bitmap: u32]      - Which of 32 slots are occupied
  [count: u32]       - Number of elements in this subtree
  [padding: 24 bytes]
  [slots: ...]       - Compressed array (only occupied slots)

Key operations:
- hamt_get(node, hash, key) -> value
- hamt_set(node, hash, key, value) -> new_node (clone-on-write)
- hamt_remove(node, hash, key) -> new_node

Primitives to expose:
- march.map.new -> map
- march.map.get map key -> value
- march.map.set map key value -> map (returns new map)
- march.map.remove map key -> map

## Phase 2: RRB-Tree - Persistent Vectors

Why after HAMT:
- More complex (relaxed balancing rules)
- Efficient concat is tricky
- But gives us O(log n) persistent arrays

Memory layout:
RRB Node Header (32 bytes):
  [count: u64]       - Number of elements
  [height: u8]       - Tree height (0 = leaf)
  [relaxed: u8]      - Is this a relaxed node?
  [padding: 22 bytes]
  [slots: ...]       - 32 child pointers or values

Primitives:
- march.array.imm.set array index value -> array
- march.array.imm.push array value -> array
- march.array.imm.concat array1 array2 -> array (efficient!)

## Implementation Strategy

Start small:
1. Implement HAMT in src/hamt.c + src/hamt.h
2. Add primitives in src/primitives.c
3. Write assembly wrappers in kernel/x86-64/hamt-*.asm
4. Test thoroughly before moving to RRB-trees

Questions

- For hashing: should we use a simple hash (FNV-1a) or something more sophisticated?


