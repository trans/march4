# March Development Log


## 2025-10-31 - Evening Session 2

**Fixed ALLOC primitive:**
- Removed spurious `pop rbx` instruction that was corrupting the stack
- ALLOC now compiles and works correctly
- The comment already said rbx doesn't need saving (callee-saved), but code was still popping it
- Test programs with `alloc` now compile without hanging

**Status:** ALLOC primitive is complete and functional. Compilation works end-to-end.

---

## 2025-10-31 - Evening

last thin you said to me: "You're right - we don't need to save rbx at all! It's callee-saved, so malloc won't clobber it. We're wasting time saving and restoring it."

## 2025-10-31 - Current Status

**Where we are:**
- ✅ Slot-based memory management complete and working
- ✅ 45 assembly primitives including FREE
- ✅ Type checking with polymorphic type variables (a-z)
- ✅ Quotations, conditionals (if), loops (times)
- ✅ Compile-time allocation tracking, automatic FREE emission
- ✅ **Design B: Token storage and monomorphization** (Phases 1-2 complete)
  - Words stored as tokens, not compiled at definition
  - Monomorphization at call-site with concrete types
  - Polymorphic words work with explicit type signatures

**What's next:**
- Phase 3: Specialization caching (avoid recompilation)
- Phase 5: Loader integration (execute token-based words)
- Runtime slot infrastructure for FREE primitive

**Key files:**
- `CONSIDER.md` - Design B architecture from ChatGPT
- `LOG.md` - Development history
- `PROGRESS.md` - Feature summary
- `docs/STATUS.md` - Comprehensive status

---

## 2025-10-31 - Design B: Token Storage & Monomorphization (Phases 1-2)

**Goal:** Enable polymorphic words without whole-program analysis by storing word definitions as tokens and compiling them lazily at call-sites with concrete types.

**Implemented:**

**Phase 1: Token Storage**
- Modified `compile_definition()` to collect tokens instead of compiling
- Created `word_definition_t` structure: stores name, tokens array, type signature
- Words stored in `comp->word_defs[]` cache during compilation
- Placeholder dict entries added (NULL addr indicates uncompiled word)
- Dictionary extended with `word_def` field for token storage

**Phase 2: Call-Site Compilation (Monomorphization)**
- Implemented `word_compile_with_context(comp, word_def, input_types, input_count)`
  - Takes stored tokens and concrete types from call-site
  - Compiles word body with type context
  - Emits FREE for non-returned slots
  - Returns compiled blob
- Modified `compile_word()` to detect uncompiled words via `entry->word_def`
- Extracts concrete types from current type stack
- Calls `word_compile_with_context()` to generate specialized version
- Stores compiled blob in database with concrete type signature
- Emits CID reference to specialized version

**Code changes:**
- `compiler.h`: Added `word_definition_t` structure, word_defs cache
- `compiler.c`:
  - Token collection in `compile_definition()` (replaces immediate compilation)
  - `word_compile_with_context()` for monomorphization
  - Call-site detection and compilation in `compile_word()`
- `dictionary.h/c`: Added `word_def` field to dict_entry, updated `dict_add()`
- Forward declaration for `word_compile_with_context()`

**Test results:**
```
$ marchc -v test_ddup.march
Defining word: ddup (collecting tokens)
  Stored type signature with 1 inputs → 3 outputs
  captured to word: type=2 text='dup'
  captured to word: type=2 text='dup'
  Stored 2 tokens in word definition cache ✓
```

**Current limitation:**
- Polymorphic words still require explicit type signatures (`$ a -> a a a ;`)
- Loader integration incomplete - can't execute token-only words yet (Phase 5)
- No specialization cache yet - recompiles on each use (Phase 3)

**Key insight:** Words are now like quotations - stored as tokens, compiled when used with concrete types. This is the foundation for true polymorphism without whole-program analysis.

**Next:** Phase 3 (specialization cache) and Phase 5 (loader integration).

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
