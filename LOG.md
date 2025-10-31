# March Development Log

## 2025-01-30 - Current Status

**Where we are:**
- ✅ Slot-based memory management complete and working
- ✅ 45 assembly primitives including FREE
- ✅ Type checking with polymorphic type variables (a-z)
- ✅ Quotations, conditionals (if), loops (times)
- ✅ Compile-time allocation tracking, automatic FREE emission
- ✅ Per-word AOT compilation (with explicit type sigs for polymorphic words)

**What's next:**
- 🎯 Design B: Monomorphization on first use
  - Store word tokens instead of compiling at definition
  - Compile at call site with concrete types
  - Cache specializations by type signature
  - Enables `ddup` to work without explicit type sig

**Key files:**
- `CONSIDER.md` - Design B architecture from ChatGPT
- `LOG.md` - Development history
- `PROGRESS.md` - Feature summary
- `docs/STATUS.md` - Comprehensive status

---

## 2025-01-30 - Slot-based Memory Management - COMPLETE ✓

**Fully Implemented:**
- Compile-time slot allocator with slot reuse
- Type stack tracks `{type, slot_id}` for each value
- Immediate handlers (drop, dup, swap, over, rot) preserve slot_ids
- FREE primitive (stub - runtime impl deferred)
- Automatic FREE emission at `;` for non-returned slots

**How it works:**
1. String allocation → `push_heap_value()` allocates slot, tracks in `slot_used[]`
2. Stack ops preserve slot_ids (e.g., `dup` copies entire entry including slot_id)
3. At `;`: compare allocated slots vs returned slots, emit FREE for difference
4. Runtime: FREE(slot_id) will read slots[slot_id] and free() (TODO: runtime infrastructure)

**Test results:**
- `"hello" drop` → ALLOC slot=0, FREE slot=0 ✓
- `"hello"` (returned) → ALLOC slot=0, no FREE ✓
- `"keep" "drop" drop` → ALLOC slot=0/1, FREE slot=1 only ✓

**Key insight:** Requires explicit type sigs for polymorphic words (e.g., `$ a -> a a a` for `ddup`).

**Next:** Pivot to Design B (store word tokens, compile on first use, cache specializations).

---

## 2025-01-30 - Slot-based Memory Management - Design

**Problem discovered:** Immediate stack primitives break user-defined words without type context.

**Solutions considered:**

## 2025-01-30 - Slot-based Memory Management Design

**Problem discovered:** Immediate stack primitives (dup, drop, swap) break user-defined words. Example: `: ddup dup dup ;` fails because immediate handlers execute during definition with no type context.

**Solutions considered:**
1. **Compile at call site (AOT):** Store tokens, compile when called → Breaks FORTH interactive model
2. **Runtime regions:** Per-word arenas with PROMOTE/FREE_REGION → Runtime overhead
3. **Compile-time refcounting:** Already implemented, but has same immediate handler problem
4. **Slot-based allocation tracking:** ✓ Chosen approach

**Selected solution: Slot-based allocation tracking**
- Compile-time: Allocate "slots" (like local variables) for heap allocations
- Each allocation gets a slot: `ALLOC("hello", slot[0])`
- Type stack tracks: `{type, slot_id}`
- At `;`: emit `FREE(slot_id)` for non-returned allocations
- Runtime: Slots are pointers in word's stack frame, just deref to free
- Overhead: Minimal (pointer array per word, simple slot allocation)

**Key insight:** Meta-refs are compile-time slot indices that map to runtime pointers. Compiler does register allocation for heap references.

**Status:** Designed, about to implement.

---

## 2025-10-29 - Compile-Time Reference Counting Implementation

Implemented zero-overhead memory management through compile-time analysis:
- Extended `type_stack_entry_t` to track allocation IDs
- Added `alloc_refcounts[]` table for compile-time tracking
- Created immediate handlers for stack primitives (drop, dup, swap, over, rot)
- Handlers manipulate refcounts at compile time, emit FREE when RC=0
- All handlers include March-equivalent comments for future self-hosting migration

**Key insight:** Stack primitives need immediate mode to preserve value identity during compile-time analysis.

Status: Working but discovered architectural issue with user-defined words.
