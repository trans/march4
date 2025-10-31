# March α₄ - Current Status

**Last Updated:** 2025-10-29

## 📊 Current State Overview

### ✅ WORKING: Core Infrastructure

**1. Compilation Pipeline (C)**
- ✅ Token-based lexer (tokens.c/h) - whitespace-delimited with string support
- ✅ Type-aware compiler (compiler.c/h) - static type checking, overload resolution
- ✅ Content-addressable database (database.c/h) - SQLite-backed CID storage
- ✅ Dictionary system (dictionary.c/h) - word lookup, type signatures
- ✅ Direct-threaded VM (kernel/x86-64/vm.asm)
- ✅ Loader/linker (loader.c/h) - CID → executable code
- ✅ ~6,400 lines of C code

**2. Runtime Primitives (45 Assembly Implementations)**
```
Arithmetic: add, sub, mul, div, mod
Bitwise: and, or, xor, not, lshift, rshift, arshift
Comparison: eq, ne, lt, le, gt, ge, zerogt, zerolt, zerop
Logic: land, lor, lnot
Stack: drop, dup, swap, over, rot
Return stack: tor(>r), fromr(r>), rfetch(r@), rdrop, twotor(2>r), twofromr(2r>)
Control: branch, 0branch, execute
Memory: fetch(@), store(!), cfetch(c@), cstore(c!)
Special: docol, i0, vm
```

**3. Type System**
- ✅ Static type checking at compile time
- ✅ Type signatures for all words
- ✅ Overload resolution
- ✅ Type variables (a-z) for polymorphism
- ✅ Base types: i64, u64, f64, bool, ptr, str, any
- ✅ Type stack tracking during compilation

**4. Control Flow & Quotations**
- ✅ Conditionals: `if` - `(true-branch) (false-branch) if`
- ✅ Loops: `times` - counted and conditional variants
- ✅ Quotations: `( ... )` - lexical quotations (QUOT_LITERAL)
- ✅ Execute primitive for running quotations
- ✅ Nested quotations support

**5. Data Types**
- ✅ Integers (i64) with immediate encoding
- ✅ Strings with escape sequences (`\"`, `\\`)
- ✅ String literals stored as BLOB_STRING with CIDs

**6. Compile-Time Reference Counting** 🆕
- ✅ Type stack extended with allocation IDs
- ✅ Refcount table tracks heap allocations at compile time
- ✅ Immediate handlers for stack primitives (drop, dup, swap, over, rot)
- ✅ Zero runtime overhead
- ✅ Detects when to emit FREE (RC=0)
- ✅ Code structured to mirror future March implementation

**Example:**
```march
: test
  "hello" dup drop drop
;
```

Compiler output:
```
ALLOC id=1 type=6 rc=1     ← String allocated
DUP id=1 rc=2              ← RC: 1→2
CONSUME id=1 rc=1          ← RC: 2→1
CONSUME id=1 rc=0          ← RC: 1→0
[Would emit FREE for id=1]  ← Detected RC=0!
```

### ⚠️ PARTIAL: In Progress

**1. Memory Management**
- ✅ Compile-time RC tracking
- ⚠️ No runtime FREE primitive yet
- ⚠️ No actual memory deallocation
- ⚠️ Stack heap vs global store distinction not implemented

**2. Runtime Execution**
- ✅ VM can execute compiled code
- ⚠️ No REPL/interpreter mode
- ⚠️ Only compilation to database (no direct execution)

**3. Outer Interpreter**
- ✅ Immediate words infrastructure exists
- ⚠️ Not a full FORTH-style outer interpreter yet
- ⚠️ CLI-based compilation only (not self-interpreting)

### ❌ MISSING: Major Features

**1. Self-Hosting**
- ❌ Compiler written in C, not March
- ❌ No March-based outer interpreter
- ❌ No `ct-*` primitives for compile-time manipulation
- ❌ No `emit-*` primitives for code generation

**2. Memory & Heap**
- ❌ No heap allocator
- ❌ No stack heap vs global store separation
- ❌ No `freeze` operation (mutable → immutable)
- ❌ No actual memory deallocation at runtime

**3. Advanced Types**
- ❌ No arrays
- ❌ No structs/records
- ❌ No user-defined types
- ❌ No TYPE_BUF (buffers)

**4. I/O & FFI**
- ❌ No file I/O
- ❌ No console I/O (print, read)
- ❌ No FFI to C/Rust
- ❌ No network (INET.md exists but not implemented)

**5. Developer Experience**
- ❌ No REPL
- ❌ No debugger
- ❌ No error messages with source locations
- ❌ No module system
- ❌ No package manager

**6. Optimization**
- ❌ No inlining
- ❌ No dead code elimination
- ❌ No constant folding
- ❌ Direct threading works but not optimized

### 📁 Test Coverage
- 33 test files in `/test/`
- Tests cover: conditionals, loops, quotations, execute, primitives
- ✅ String tests passing
- ✅ RC tracking tests passing
- ⚠️ No automated test runner

### 📐 Architecture Quality

**Strengths:**
- Clean separation: compiler, loader, VM
- CID-based content addressing
- Type system foundation solid
- Code well-structured for self-hosting migration

**Tech Debt:**
- Mixed legacy (cells) and new (CID/blob) systems
- Some unused infrastructure (buffer_stack, etc.)
- Debug traces still in production code
- No comprehensive error handling

---

## 🎯 Possible Next Steps

### **Path A: Complete Memory Management** ⭐ LOW-HANGING FRUIT
1. Implement runtime FREE primitive (drop.free)
2. Update consume_value() to emit FREE when RC=0
3. Add simple heap allocator (or use malloc/free for now)
4. Test actual memory deallocation
5. Implement stack heap vs global store

**Estimated effort:** 2-4 hours
**Immediate benefit:** Strings actually get freed, validates compile-time RC

### **Path B: Move Toward Self-Hosting**
1. Implement outer interpreter loop in March
2. Add `ct-*` primitives (ct-stack-pop, etc.)
3. Translate immediate handlers from C to March
4. Build March-based compiler

**Estimated effort:** 20-40 hours
**Long-term benefit:** Foundation for true self-hosting

### **Path C: Add Essential Features**
1. Implement print/output primitives
2. Add REPL for interactive development
3. File I/O for loading programs
4. Better error messages

**Estimated effort:** 8-16 hours
**Immediate benefit:** Interactive development possible

### **Path D: Expand Type System**
1. Implement arrays
2. Add structs/records
3. User-defined types
4. More sophisticated type inference

**Estimated effort:** 16-32 hours
**Long-term benefit:** Rich data structures

### **Path E: Clean Up & Stabilize**
1. Remove legacy cell encoding
2. Comprehensive error handling
3. Automated test suite
4. Documentation updates

**Estimated effort:** 8-12 hours
**Immediate benefit:** More maintainable codebase

---

## 🏗️ Current Architecture

```
Source File (.march)
        ↓
    Tokenizer (tokens.c)
        ↓
    Compiler (compiler.c)
    ├─ Type checker
    ├─ Immediate word handlers
    ├─ Compile-time RC tracking
    └─ Code emission
        ↓
    Database (march.db)
    ├─ Blobs (compiled code, strings)
    ├─ Words (definitions)
    └─ Type signatures
        ↓
    Loader (loader.c)
    ├─ CID → address resolution
    └─ Linking
        ↓
    VM (kernel/x86-64/vm.asm)
    └─ Direct-threaded execution
```

---

## 📝 Recent Work

**Compile-Time Reference Counting Implementation (2025-10-29)**

Implemented zero-overhead memory management through compile-time analysis:

1. Extended `type_stack_entry_t` to track allocation IDs
2. Added `alloc_refcounts[]` table for compile-time tracking
3. Created immediate handlers for stack primitives:
   - `compile_drop()` - decrements RC, emits FREE when RC=0
   - `compile_dup()` - increments RC
   - `compile_swap()`, `compile_over()`, `compile_rot()` - preserve allocation identity
4. Registered as immediate words, overriding runtime primitives
5. All handlers include March-equivalent comments for future migration

**Key insight:** Stack primitives need immediate mode to preserve value identity during compile-time analysis. This architecture maps cleanly to self-hosted implementation.

---

## 🔮 Vision: Self-Hosted March

**The Goal:**
A March compiler written in March, using immediate words to manipulate compile-time state.

**The Path:**
1. Implement runtime memory management (Path A)
2. Add essential I/O for bootstrapping (Path C subset)
3. Build outer interpreter in March (Path B)
4. Translate compiler piece by piece from C to March
5. Eventually: Pure March compiler compiling itself

**The Philosophy:**
Context-oriented programming - no AST, just tokens interpreted in different contexts (compile-time vs runtime). Immediate words ARE the compiler.
