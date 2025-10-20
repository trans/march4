# Summary: Global Store Architecture and FFI Plan

*Goal*: Create an in-memory immutable global store with periodic persistence
to SQLite, using OCaml for the compiler/tooling, with optional Rust FFI
for high-performance persistent data structures. Eventually, March
self-hosts and replaces OCaml.

## 1. Architectural Overview

• The global store is a content-addressed graph of immutable nodes.
• Lives in memory during execution; persisted to SQLite on commits.
• SQLite only stores *snapshots*, not every operation.
• Objects are hash-identified (e.g. BLAKE3 → CID).

Memory layers:
  - Workspace heap: mutable working data (stack’s heap)
  - Global store: immutable, content-addressed nodes (in memory)
  - SQLite: persistent snapshot of CIDs for durability

## 2. Language Roles

• OCaml — compiler, typechecker, linker, and global store manager.
• Rust — provides fast persistent data structures (HAMT, vector tries)
  via FFI. (Later replaced by March.)
• March — eventual self-hosted language reusing same design.

## 3. OCaml Phase → Rust FFI Phase → March Phase

Phase 1 (OCaml only):
  - Use OCaml’s built-in Map/Set for persistent structures.
  - Persist snapshots to SQLite.

Phase 2 (OCaml + Rust FFI):
  - Replace OCaml Maps/Sets with Rust-backed HAMT/Vectors.
  - Keep OCaml interfaces identical; only backend changes.
  - Rust manages refcounts; OCaml sees opaque handles.

Phase 3 (March self-host):
  - March compiler/runtime implemented in March/Zig.
  - March directly calls the Rust DS FFI or reimplements it natively.
  - Architecture remains the same: immutable in-memory store,
    persisted CAS snapshots.

## 4. Rust <-> OCaml FFI Interface

FFI uses opaque handles (Arc-backed objects) to avoid copying data
between runtimes. Each handle is an OCaml custom block that owns an Arc.

Rust side type signatures:

```
#[repr(C)]
pub struct MapHandle(*mut MapInner);

#[no_mangle]
pub extern "C" fn map_new() -> MapHandle;
#[no_mangle]
pub extern "C" fn map_get(h: MapHandle, key: *const u8, len: usize) -> OptionHandle;
#[no_mangle]
pub extern "C" fn map_insert(h: MapHandle, key: *const u8, len: usize,
                             val: ValueHandle) -> MapHandle;
#[no_mangle]
pub extern "C" fn map_size(h: MapHandle) -> u64;
#[no_mangle]
pub extern "C" fn map_drop(h: MapHandle); // decref

// optional: blob + cid support
#[no_mangle]
pub extern "C" fn blob_from_bytes(ptr: *const u8, len: usize) -> BlobHandle;
#[no_mangle]
pub extern "C" fn blob_len(h: BlobHandle) -> u64;
#[no_mangle]
pub extern "C" fn cid_of(h: MapHandle, out: *mut u8); // writes 32 bytes
```

OCaml binding via ctypes:

```
open Ctypes
open Foreign

type map
let map : map structure typ = structure "MapHandle"

let map_new = foreign "map_new" (void @-> returning (ptr map))
let map_get = foreign "map_get" (ptr map @-> string @-> returning (ptr_opt map))
let map_insert = foreign "map_insert" (ptr map @-> string @-> ptr map @-> returning (ptr map))
let map_size = foreign "map_size" (ptr map @-> returning uint64_t)
```

OCaml GC finalizer decrements the Arc count via `map_drop`.

## 5. Global Store Lifecycle

• Root structure holds top-level maps/vectors (globals, types, etc.)
• On commit:
    - Hash each modified object (SHA256 or BLAKE3)
    - Insert new entries into SQLite if CID not already present
    - Write new root manifest to SQLite
• On load:
    - Read latest manifest
    - Lazily hydrate in-memory structures from SQLite as needed
    - Maintain an LRU for cold objects

## 6. Future Optimizations

• Hybrid GC / refcount system for short-lived workspace mutables.
• Automated “freeze” insertion when values escape scope.
• Incremental commits (dirty-node tracking).
• Move Rust DS implementation into March when self-hosted.


## Implementation Priorities

1. Start with OCaml-only persistent maps and sets.
2. Integrate SQLite commit/load flow.
3. Add Rust HAMT/Vector FFI for hot code paths.
4. Transition tooling to March over time.

End of Summary & Sketch
