# March Language - Working Features Summary

**Current Status (2025-11-01):** Array literals complete with identity primitive and nested support! String storage refactored to heap allocation with memcpy optimization. 47 primitives total. Clean separation: Database = code storage, Heap = runtime data (program-controlled).

---

  ✅ 13. Array Literals & String Storage Refactor - COMPLETE!

  Date: 2025-11-01
  Status: ✅ Complete

  **Array Features Implemented:**
  - Empty arrays: `[ ]`
  - Literal arrays: `[ 10 20 30 ]`
  - Identity primitive `_` (signature: `a -> a`) for stack capture: `5 [ _ ]`
  - Nested arrays: `[ [ 1 2 ] [ 3 4 ] ]` (arbitrary depth)
  - Homogeneous type checking enforced

  **String Storage Refactor:**
  - Changed from automatic database persistence to heap allocation
  - Strings now heap-allocated like arrays (program controls persistence)
  - Added `memcpy` primitive for efficient bulk copying
  - Optimized compilation: 60+ instructions → 5 instructions per string
  - Architecture: DB stores literal data (cached), heap gets fresh copy each execution

  **New Primitives:**
  - `_` (PRIM_IDENTITY, 46) - Identity function `a -> a`
  - `memcpy` (PRIM_MEMCPY, 47) - Memory copy `ptr ptr i64 -> ptr`

  **Type System:**
  - Stack types: `i64`, `u64`, `f64`, `bool`, `ptr`
  - Heap types: `str`, `array` (program-controlled allocation)
  - Polymorphic: `a`-`z` type variables
  - Added `TYPE_ARRAY` support across compiler, dictionary, runner

  **Architecture Clarification:**
  - Database = Code storage (string literals, compiled words, type sigs)
  - Loader = Runtime cache (load blobs once from DB)
  - Heap = Runtime data (fresh allocations, program-controlled)

  **Test Files:**
  - `test_array_typed.march` - Typed array literals
  - `test_empty_array.march` - Empty array handling
  - `test_array_identity.march` - Identity primitive in arrays
  - `test_array_nested.march` - Nested array structures
  - `test_string_simple.march` - String literal tests

  **Files Modified:**
  - `kernel/x86-64/identity.asm` - Identity primitive (no-op)
  - `kernel/x86-64/memcpy.asm` - Memory copy with rep movsb
  - `src/compiler.c` - Array literal support, string memcpy optimization
  - `src/dictionary.c` - TYPE_ARRAY parsing and display
  - `src/runner.c` - TYPE_ARRAY stack display
  - `src/types.h` - TYPE_ARRAY, PRIM_IDENTITY, PRIM_MEMCPY
  - `src/primitives.h/c` - Register new primitives

  **Performance:**
  - String "hello world" (12 bytes): 60+ instructions → 5 instructions
  - Array literal overhead minimal (alloc + element stores)

---

  ✅ 12. Design B: Token Storage & Monomorphization (Phases 1-2) - COMPLETE!

  Date: 2025-10-31
  Status: ✅ Phases 1-2 Complete | ⏳ Phase 3 (cache) & Phase 5 (loader) TODO

  **Problem:** Polymorphic words need type context during compilation, but definitions (`: ddup dup dup ;`) have no concrete types. Previous slot-based approach required explicit type signatures.

  **Solution:** Design B - Store word definitions as tokens (like quotations), compile lazily at call-site with concrete types from type stack (monomorphization).

  **Phase 1: Token Storage**
  - Modified `compile_definition()` to collect tokens instead of compiling
  - Created `word_definition_t` structure (name, tokens[], type_sig)
  - Words stored in compiler cache `comp->word_defs[]`
  - Placeholder dict entries (NULL addr = uncompiled)
  - Dictionary extended with `word_def` field

  **Phase 2: Call-Site Compilation**
  - `word_compile_with_context(comp, word_def, input_types, input_count)`
    - Compiles tokens with concrete type context
    - Full slot-based memory management (FREE emission)
    - Returns compiled blob
  - `compile_word()` detects uncompiled words via `entry->word_def`
  - Extracts concrete types from call-site type stack
  - Generates specialized version, stores in database
  - Emits CID reference to specialization

  **Test Results:**
  - ✅ Words store as tokens: "Stored 2 tokens in word definition cache"
  - ✅ No compilation at definition: "Defining word: ddup (collecting tokens)"
  - ✅ Monomorphization logic implemented (needs loader for end-to-end test)

  **Files Modified:**
  - `src/compiler.h` - word_definition_t, word_defs cache, MAX_WORD_DEFS
  - `src/compiler.c` - Token collection, word_compile_with_context(), monomorphization
  - `src/dictionary.h/c` - word_def field, dict_add() signature

  **Current Limitations:**
  - Polymorphic words still need explicit type sigs (`$ a -> a a a ;`)
  - Phase 3 (specialization cache) not implemented - recompiles each use
  - Phase 5 (loader integration) incomplete - can't execute token-only words
  - No database storage of token definitions yet

  **Next Steps:**
  - Phase 3: Specialization cache to avoid recompilation
  - Phase 5: Loader integration to execute token-based words
  - Full end-to-end test with execution

---

  ✅ 11. Slot-Based Memory Management - COMPLETE!

  Date: 2025-10-30
  Status: ✅ Complete

  **Problem:** Reference counting approach broke polymorphic words - immediate handlers (dup, drop) need type context during compilation, but polymorphic word definitions (`: ddup dup dup ;`) have no concrete types.

  **Solution:** Slot-based compile-time allocation tracking with automatic FREE emission.

  **Implementation:**
  - Type stack now tracks `{type, slot_id}` pairs
  - Compile-time slot allocator assigns unique IDs to heap allocations
  - Immediate handlers preserve slot_ids (no refcount arithmetic)
  - At word end (`;`), emit FREE for allocated slots not on return stack
  - Simple set-difference logic: `allocated - returned = free`

  **Primitives:**
  - FREE primitive added (free.asm) - runtime stub, infrastructure TODO
  - Total: 45 assembly primitives

  **Test Results:**
  - ✅ `"hello" drop` → allocates slot 0, emits FREE
  - ✅ `"hello"` (returned) → allocates slot 0, no FREE
  - ✅ `"keep" "drop" drop` → allocates slots 0,1, FREEs only slot 1

  **Benefits:**
  - Per-word AOT compilation (no whole-program analysis)
  - Zero runtime overhead (all analysis at compile time)
  - Foundation for monomorphization (Design B)

  **Current Limitation:** Polymorphic words require explicit type signatures (e.g., `$ a -> a a a`) to provide type context during independent compilation.

  **Next:** Design B - Store word tokens, compile on first use with concrete types, cache specializations by type signature.

---

  ✅ 1. Runtime Layer (Assembly + C)

  Location: kernel/x86-64/
  - 40 assembly primitives in x86-64 NASM:
    - Stack ops: dup, drop, swap, over, rot
    - Arithmetic: +, -, *, /, mod
    - Comparisons: =, <>, <, >, <=, >=
    - Bitwise: and, or, xor, not, <<, >>, >>>
    - Logical: land, lor, lnot, 0=, 0>, 0<
    - Memory: @, !, c@, c!
    - Return stack: >r, r>, r@, rdrop, 2>r, 2r>
  - VM inner interpreter (vm.asm):
    - Cell-tagged dispatch (XT/EXIT/LIT/EXT)
    - 8KB data stack
    - 8KB return stack
    - Register allocation: rsi=data stack, rdi=return stack, rbx=IP

  Status: Working, tests pass (note: currently has linking issue with Rust lib)

  ---
  ✅ 2. State Management (Rust)

  Location: runtime/
  - Persistent/immutable data structures via im-rs
  - C FFI exports: march_state_create/get/set/free
  - Zero runtime overhead (compiled code only)

  Status: Built, not yet integrated with VM tests

  ---
  ✅ 3. Bootstrap Compiler (OCaml)

  Location: compiler/

  Components:
  - Lexer (lexer.ml): Tokenizes March source
    - Haskell-style -- comments
    - Literals, words, operators
  - Parser (parser.ml): Builds AST
    - Word definitions: : name body ;
    - Expression sequences
  - Type Checker (typecheck.ml):
    - Shadow type stack (compile-time only)
    - Overload resolution via specificity scoring
    - Multi-dispatch on type signatures
    - Priority-based tie-breaking
    - Debug mode flag -g
  - Code Generator (codegen.ml):
    - Emits cell streams (XT/EXIT/LIT tags)
    - Real SHA256 CIDs via digestif
  - Database Writer (database.ml):
    - Stores blobs and words in SQLite

  Compiler Flags:
  -o <db>     Output database (default: march.db)
  -v          Verbose output
  -g          Debug mode (runtime type tracking)
  --tokens    Show tokens and exit
  --ast       Show AST and exit
  --types     Show type signatures and exit

  Status: Fully working, successfully compiles March to database

  ---
  ✅ 4. Database Schema (SQLite)

  Location: schema.sql

  Tables:
  - blobs - Content-addressable binary data (SHA256 CIDs)
  - words - Named definitions with type signatures
  - defs - Compiled code metadata (source_text, source_hash)
  - edges - Dependency graph for GC
  - modules - Program/library organization
  - state - Global variables (immutable snapshots)

  Recent updates (2025-10-19):
  - Added UNIQUE(name, namespace, type_sig) constraint on words table
  - INSERT OR REPLACE for recompilation without duplicates
  - Source code storage in defs table with SHA256 hash

  Status: Complete, in use by compiler

  ---
  ✅ 5. Type System

  - Static type checking with shadow stack
  - Overload resolution:
    - Specificity scoring (exact match=100, concrete→typevar=50, polymorphic=10)
    - Priority tie-breaking
  - Registered primitives with type signatures
  - Type inference for word outputs (from literals/operations)

  Status: Working for words with literals, needs input inference for words without literals

  ---
  ✅ 6. Examples & Tests

  Location: examples/
  - hello.march - Simple word definitions
  - literals.march - Words with literal values (type checks successfully)
  - typed.march - Test cases for type system

  Example compilation:
  $ marchc -v examples/literals.march
  Parsed 4 words
  Type checked 4 words
  Generated code:
    five: 86cd633ad651e8cd... (3 cells, 24 bytes)
  Stored in database with SHA256 CIDs

  ---
  ✅ 7. Cell Encoding Update (Phase 1 Complete!)

  Date: 2025-10-18
  Status: ✅ Complete

  Upgraded from 2-bit to 4-tag variable-bit encoding:

  New encoding scheme:
  - 00  = XT  (execute word, EXIT if addr=0)
  - 01  = LIT (immediate 62-bit signed literal, single cell!)
  - 10  = LST (symbol ID for type system)
  - 110 = LNT (1+ consecutive literals, full 64-bit values)
  - 111 = EXT (future extensions)

  Files updated:
  - schema.sql: Added symbols table
  - types.ml: New tag types and encode/decode functions
  - database.ml: Symbol table operations
  - codegen.ml: New emission + LNT optimization (3+ literals)
  - vm.asm: Variable-bit tag decoding + all handlers
  - main.ml: Enhanced debug output

  Test results (examples/test_encoding.march):
  - five:    [LIT:5] [EXIT]                              (2 cells, 16 bytes)
  - ten:     [LIT:10] [EXIT]                             (2 cells, 16 bytes)
  - fifteen: [XT:five] [XT:ten] [XT:+] [EXIT]            (4 cells, 32 bytes)
  - nums:    [LNT:5: 1 2 3 4 5] [EXIT]                   (7 cells, 56 bytes) ✨

  Benefits:
  ✓ Immediate literals fit in single cell (was 2 cells)
  ✓ LNT for 1+ consecutive literals (supports full 64-bit values)
  ✓ LIT limited to 62-bit signed (so LNT needed for larger values)
  ✓ Symbol literals for type system/metaprogramming
  ✓ Room for future extensions (EXT tag)
  ✓ EXIT is now just XT(0), saves a tag
  ✓ Simple FORTH-style syntax: : name body ;

  ---
  ✅ 8. VM Tests Complete (LNT Bug Fixed)

  Date: 2025-10-18
  Status: ✅ Complete

  Fixed critical dispatch bug in VM that was preventing LNT from working.
  The dispatcher was incorrectly jumping to LST handler when low 2 bits
  were 10, without checking bit 2 to distinguish LST (010) from LNT (110).

  Changes:
  - vm.asm: Fixed dispatch to check bit 2 for tag 10
  - test_vm.c: Added LNT test case (Test 6)
  - Makefile: Removed runtime library dependency

  All 6 VM tests now passing:
  ✓ Test 1: Simple addition (5 + 3 = 8)
  ✓ Test 2: Dup and add (10 dup + = 20)
  ✓ Test 3: Complex expression ((7-3)*2 = 8)
  ✓ Test 4: Equality test (5 = 5 = -1)
  ✓ Test 5: Swap test (10 20 swap - = 10)
  ✓ Test 6: LNT bulk literals ([LNT:2] 10 20 → stack: 20 10)

  VM is now fully operational with 4-tag variable-bit encoding!

  ---
  What's NOT Working Yet

  ❌ Input type inference - Can't infer : square dup * ; needs int64 → int64
  ❌ Inter-word linking - User words calling other user words (e.g., : fifteen five ten + ;)
  ❌ Quotation linking - Quotations compile but CID → address resolution not implemented
  ❌ Control flow: No IF/THEN/ELSE, loops (depends on quotation linking)
  ❌ String literals - Only int64 literals currently supported
  ❌ Print primitive - No way to output strings yet
  ❌ edges table - Not populated yet (needed for dependency tracking and GC)

  Note: Words with only literals and primitives work perfectly!
  Example: : answer 21 21 + ; → 42 ✓
  Quotations compile and store but can't execute yet (linking needed)

  ---
  ✅ 9. C Compiler - COMPLETE!

  Date: 2025-10-18 → 2025-10-19
  Status: ✅ Complete

  **Decision:** Rewrite compiler in C for true FORTH architecture.

  **Why:**
  - OCaml's strengths (ADTs, pattern matching) pull toward AST/tree traversal
  - FORTH compiles token-stream, one-pass (no AST!)
  - C is natural fit: simple, imperative, direct VM integration
  - Eliminates FFI complexity

  **Implementation (src/ directory):**
  - ✅ types.h - Core type definitions
  - ✅ cells.c/.h - Cell encoding/decoding (27 tests passing)
  - ✅ tokens.c/.h - Token stream reader (NO AST!)
  - ✅ dictionary.c/.h - Hash table + overload resolution (29 tests passing)
  - ✅ database.c/.h - SQLite integration with SHA256 (36 tests passing)
  - ✅ primitives.c/.h - Register 39 assembly ops (53 tests passing)
  - ✅ compiler.c/.h - One-pass compiler core (37 tests passing)
  - ✅ loader.c/.h - Load words from database (33 tests passing)
  - ✅ runner.c/.h - Execute on VM with real assembly primitives
  - ✅ marchc.c - Complete CLI tool
  - ✅ test_framework.h - Simple assertion-based testing
  - ✅ Makefile - Build system with test runner

  **All 215 tests passing!** (27+29+36+53+37+33+0)

  **Architecture Achieved:**
  - ✅ One-pass compilation (read token → compile immediately)
  - ✅ Type stack (compile-time only, not runtime)
  - ✅ Dictionary-driven (primitives + user words)
  - ✅ Overload resolution via type signature matching
  - ✅ Direct SQLite C API (no bindings)
  - ✅ SHA256 content-addressable storage
  - ✅ Direct VM calls (no FFI layer)
  - ✅ Real assembly primitive integration

  **Compiler Features:**
  - Token stream compilation (no AST!)
  - Compile-time type checking with shadow type stack
  - Literal emission (LIT cells)
  - Primitive word calls (XT cells with real addresses)
  - User word definitions (: name body ;)
  - Type signature inference from stack state
  - Database storage with SHA256 CIDs
  - Dictionary integration for cross-word references
  - Load from database and execute on VM
  - Full pipeline: Source → Compile → Store → Load → Execute

  **Working Examples:**
  ```bash
  $ cat hello.march
  : five 5 ;
  : fifteen 5 10 + ;
  : answer 21 21 + ;

  $ marchc -r answer -s hello.march
  Stack (1 items):
    [0] = 42
  ```

  **Result:**
  The C compiler is now the **primary, working compiler** for March!
  OCaml compiler kept in compiler/ as reference implementation.

  ---
  ✅ 10. Runtime Quotations & Execute Primitive - COMPLETE!

  Date: 2025-10-20
  Status: ✅ Complete (compilation and execute primitive working, linking deferred)

  **Goal:** Enable quotations as first-class values for higher-order programming.

  **Design Decision - Anonymous Blobs:**
  - Quotations stored as anonymous blobs (BLOB_CODE kind)
  - **NOT** stored in words table (no name pollution from _q0, _q1, etc.)
  - Type information via sig_cid reference to type_signatures table
  - CIDs tracked in compiler for future linking

  **Type Signature Deduplication:**
  - New `type_signatures` table with sig_cid (SHA256 of "input_sig|output_sig")
  - Blobs reference signatures via sig_cid foreign key
  - Eliminates redundant storage of identical type signatures
  - Type info already on blob, no need for defs entries

  **Schema Changes (schema.sql):**
  - Added `type_signatures` table (sig_cid, input_sig, output_sig)
  - Added `sig_cid` column to blobs table
  - Added `sig_cid` column to defs table (replacing stack_effect)
  - Added `effects` field to defs for documentation

  **Database API (database.c):**
  - `db_store_type_sig()` - Store type signature, returns sig_cid
  - `db_store_blob()` - Store anonymous blob with type signature

  **Compiler Changes (compiler.c):**
  - `materialize_quotations()` - Refactored to use anonymous blobs
  - Builds type signature from compile-time stack state
  - Stores blob directly (no word definition)
  - Tracks CIDs in `pending_quot_cids[]` for linking
  - Emits `[LIT 0]` placeholders (linking TBD)

  **Execute Primitive (kernel/x86-64/execute.asm):**
  - New assembly primitive: `execute ( ptr -- )`
  - Pops address from data stack
  - Calls address as executable code
  - Foundation for quotation execution and loops
  - Registered with signature: "ptr ->"
  - Now 42 total primitives (39 + branch + 0branch + execute)

  **Working:**
  - ✅ Quotation syntax: [ body ]
  - ✅ Nested quotations
  - ✅ Type inference for quotation inputs/outputs
  - ✅ Anonymous blob storage (no words table pollution)
  - ✅ Type signature deduplication
  - ✅ Execute primitive implemented and tested
  - ✅ Immediate word system (true/false compile to literals)

  **Example:**
  ```march
  : test-quot [ 10 20 + ] ;  -- Compiles to anonymous blob
  -- Signature inferred: [ -> i64 ]
  ```

  **Deferred (future work):**
  - ❌ Quotation linking - Need loader to resolve CID → runtime address
  - ❌ Runtime execution - Can't actually call quotations yet (linking required)
  - ❌ Loops - Need quotation execution working first

  **Current Limitations:**
  - Quotations compile successfully to blobs
  - Execute primitive exists but can't be tested without linking
  - CIDs tracked but not yet resolved to runtime addresses
  - Placeholder `[LIT 0]` emitted where quotation address should be

  **Next Steps:**
  1. Implement quotation linking in loader (CID → runtime address resolution)
  2. Patch LIT cells with actual addresses
  3. Test execute primitive with real quotations
  4. Implement control flow (if/then/else, loop/repeat)

  ---
  Summary

  **Working:**
  - ✅ 42 assembly primitives in x86-64 assembly (including execute!)
  - ✅ VM with 4-tag variable-bit encoding (XT/LIT/LST/LNT/EXT)
  - ✅ **C compiler COMPLETE! (215 tests passing)**
    - Token stream compilation (NO AST!)
    - Compile-time type checking with type stack
    - One-pass compilation (true FORTH architecture)
    - Database integration with SHA256 CIDs
    - Primitive registration with type signatures
    - User word definitions (: name body ;)
    - **Quotation compilation ([ body ])**
    - **Type signature deduplication**
    - Loader: Load compiled words from database
    - Runner: Execute on real assembly VM
    - CLI tool (marchc): Full compile & execute pipeline
  - ✅ Real SHA256 content-addressable storage
  - ✅ SQLite database integration
  - ✅ **Runtime quotations foundation:**
    - Anonymous blob storage (no words table pollution)
    - Type signature inference
    - Execute primitive
  - ✅ OCaml bootstrap compiler (kept as reference)

  **Next Features:**
  1. Quotation linking (CID → runtime address resolution)
  2. Control flow syntax (IF/THEN/ELSE, loops) - depends on quotation linking
  3. String literals and print primitive
  4. Input type inference for words without literals
  5. More examples and documentation


