# March Language - Working Features Summary

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
  - defs - Compiled code metadata
  - edges - Dependency graph for GC
  - modules - Program/library organization
  - state - Global variables (immutable snapshots)

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
  ❌ Loading/linking - Can't load compiled code from database and run it
  ❌ Control flow: No IF/THEN/ELSE, loops, etc.
  ❌ String literals - Only int64 literals currently supported
  ❌ Print primitive - No way to output strings yet

  ---
  ✅ 9. C Compiler Transition (In Progress)

  Date: 2025-10-18
  Status: 🚧 In Progress

  **Decision:** Rewrite compiler in C for true FORTH architecture.

  **Why:**
  - OCaml's strengths (ADTs, pattern matching) pull toward AST/tree traversal
  - FORTH compiles token-stream, one-pass (no AST!)
  - C is natural fit: simple, imperative, direct VM integration
  - Eliminates FFI complexity

  **Completed (src/ directory):**
  - ✅ types.h - Core type definitions
  - ✅ cells.c/.h - Cell encoding/decoding (27 tests passing)
  - ✅ tokens.c/.h - Token stream reader (no AST!)
  - ✅ dictionary.c/.h - Hash table + overload resolution (29 tests passing)
  - ✅ database.c/.h - SQLite integration with SHA256 (36 tests passing)
  - ✅ primitives.c/.h - Register 39 assembly ops (53 tests passing)
  - ✅ compiler.c/.h - One-pass compiler core (37 tests passing)
  - ✅ test_framework.h - Simple assertion-based testing
  - ✅ Makefile - Build system with test runner

  **All 182 tests passing!**

  **Architecture:**
  - One-pass compilation (read token → compile immediately)
  - Type stack (compile-time only, not runtime)
  - Dictionary-driven (primitives + user words)
  - Overload resolution via type signature matching
  - Direct SQLite C API (no bindings)
  - SHA256 content-addressable storage
  - Direct VM calls (no FFI layer)

  **Compiler Features:**
  - Token stream compilation (no AST!)
  - Compile-time type checking with shadow type stack
  - Literal emission (LIT cells)
  - Primitive word calls (XT cells with addresses)
  - User word definitions (: name body ;)
  - Type signature inference from stack state
  - Database storage with SHA256 CIDs
  - Dictionary integration for cross-word references

  **Pending:**
  - loader.c/.h - Load + link words from database
  - runner.c - Execute compiled code (direct vm_run call)
  - marchc.c - Main entry point

  **Plan:**
  Keep OCaml compiler in compiler/ as reference, build C version in src/.
  Once C compiler is working, make it default.

  ---
  Summary

  Working:
  - 39 assembly primitives in x86-64 assembly
  - VM with 4-tag variable-bit encoding (XT/LIT/LST/LNT/EXT)
  - OCaml bootstrap compiler (functional, uses AST)
  - OCaml loader + runner (via FFI)
  - **C compiler core complete! (182 tests passing)**
    - Token stream compilation (no AST!)
    - Compile-time type checking
    - Database integration with SHA256
    - Primitive registration
    - User word definitions
  - Real SHA256 content-addressable storage
  - SQLite database integration

  Next Critical Features:
  1. Word linking/relocation for inter-word calls
  2. Runner integration (load from DB → execute on VM)
  3. Control flow syntax and compilation
  4. Main CLI tool (marchc)


