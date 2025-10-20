# Design Overview: Typed Forth Variant with Database-Backed Store

Author: Thomas Sawyer & AI
Date: 2025-10-14
Topic: Core execution model, memory architecture, and compiler/runtime
       design for a statically-typed, Forth-like language integrated with
       a content-addressable database store (SQLite).

## 1. Execution Model Summary

• The system follows a traditional Forth inner interpreter, but statically typed:
  - Each word has a concrete, compile-time type signature.
  - Type checking uses a shadow type stack during compilation; runtime carries no type info.
  - Overloading and multi-dispatch occur at compile time.

• At runtime:
  - Code executes as a stream of “cells” (pre-compiled instructions).
  - Data is exchanged via a word-sized data stack (raw machine words / pointers).
  - No dynamic lookup of names; the dictionary is used only at compile/link time.

## 2. Program Representation and Database Storage

• Source words and definitions are stored in a SQLite database:
  - Tables for `words`, `definitions`, `edges`, and `blobs`.
  - Each compiled definition (code stream) is content-addressable by a hash (CID).
  - The runtime executes compiled code by CID; editing tools use names and metadata.

• Compilation workflow:
  1. Source text → token stream.
  2. Compiler resolves names → CIDs, resolves overloads by static type.
  3. Emits a code stream of *Cells* (binary format).
  4. Stores resulting object into the database with a deterministic hash.

• The runtime never needs word names—only CIDs and direct pointers.

## 3. Cell Encoding (Code Stream)

• Each Cell = one machine word (64 bits typical) with low bits used as a tag.

  Low-bit tags:
      00 = XT (execute word)
      01 = GO (return)
      10 = LIT_I62 (next word = 64-bit literal)
      11 = ?? (tbd)

• In future 11 may be used for for Extended forms (EXT) allow unlimited new instruction kinds:
      EXT + next word sub-tag (e.g., LIT_REF, LIT_F64, DBG_MARK, CALL_INDIRECT, etc.)

• Dispatch loop pseudocode:

    cell = *IP++;
    tag  = cell & 0x3;
    if tag == 0: jmp [cell & ~0x3];         // XT
    if tag == 1: IP = pop_return();         // EXIT
    if tag == 2: push(*IP++);               // LIT_I64
    if tag == 3: handle_EXT(IP);

• Alignment guarantees (x86-64, AArch64): low 2 bits of pointers are zero due to ≥4-byte alignment, so pointer tagging is safe and portable.


## 4. Data Stack and Runtime Values

• The **data stack** is fully type-erased at runtime.

  - Each slot is a machine word (`uintptr_t`).
  - Primitives know their stack effects and interpret the slots accordingly.
  - Integers and small immediates fit directly in the word.
  - Pointers reference heap objects (strings, records, arrays, etc.).
  - No `Value {tag, payload}` wrapper at runtime; static typing removes the need.

• Heap objects (“blobs”) are self-describing:
  struct Blob {
      uint32_t kind;   // type or format ID
      uint32_t flags;
      uint64_t len;
      uint8_t  data[];
  };

  - Words that expect a pointer type dereference `Blob` to read size, etc.
  - Global store objects are immutable snapshots; stack-heap objects are mutable.


## 5. Memory Model

• Two conceptual heaps:

  1. **Stack’s Heap (Workspace)**
     - Short-lived, mutable allocations for local computation.
     - Automatically freed when the word/frame returns.
     - No reference counting required; purely linear ownership.

  2. **Managed Global Store**
     - Immutable, content-addressable objects persisted in the database.
     - Used for variables, saved data, and cross-word references.
     - Updated via “freeze”: converts a mutable workspace object into an immutable snapshot, computes its hash, and stores it.

• The stack heap is where non-immediate (64 bit numbers, etc) data lives for stack processing.


## 6. Compiler / Optimizer: Automatic Mutability Inference

Goal: Let the compiler decide when in-place mutation is safe and when to
freeze automatically—eliminating the need for manual “mutable vs immutable” choice.
A program can override this marking explictly which to use.

Note: This feature does not have to be implmented right away.

Core technique: **uniqueness / borrow analysis** (SSA data-flow lattice).

Lattice per SSA value:
    ⊥ Unknown
    U Unique (sole owner, safe to mutate)
    B Borrowed / Shared (aliases exist)
    E Escapes (crosses boundary → must freeze)
    ⊤ Conflict (fallback conservative)

Key rules:
  • Fresh alloc / thaw / fetch_global → U
  • Copy / alias / merge → join → B
  • Escape (store, return, send) → E (insert freeze)
  • join(U,U)=U, join(U,B)=B, join(B,E)=E, etc.

Lowering decisions:
  • If `state(arg)==U` and value doesn’t escape → lower pure update to in-place variant (`set-name!`).
  • Otherwise → structural share (persistent update).
  • On store/return of mutable → insert freeze snapshot.

Compiler performs forward data-flow over SSA; emits warnings or inserts
freeze nodes automatically.


## 7. Optional Development Features

• Shadow type/borrow stacks for debugging builds only. (Sweet!)
• Mutability trace dump: show where values changed U→B or froze.
• assert-unique / assert-immutable primitives for tests.
• Visualization flags: `-Zmutviz` to annotate compilation output.


## 8. Interoperability and Future Extensions

• Planned C interop for primitives and FFI (useful for SQLite access).
• Deterministic, little-endian on-disk format for CAS hashing.
• Optional per-code literal side tables for literal metadata (size, type).
• EXT-encoded future features (e.g., fast strings, debug markers).
• Database GC and dead-code deletion via “edge table” reference graph.


## 9. Summary of Design Choices

✔ Code stored as content-addressed bytecode streams in SQLite
✔ One-word Cells with 2-bit low-tag encoding.
✔ Explicit EXIT instruction XT(0).
✔ Type-erased data stack: raw words / pointers, no runtime tags needed.
✔ Heap objects self-describe with headers.
✔ Mutable workspace heap (for stack ops); immutable global store (for storing program state).
✔ Automatic in-place vs persistent update via borrow/uniqueness analysis.  
✔ Late “freeze” insertion at escape boundaries.
✔ Deterministic little-endian on-disk representation.
✔ All type safety and overload resolution done statically.

See DETAILS.md for some addition information.

---
End of Document
