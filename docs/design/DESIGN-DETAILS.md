# Additional Details/Calarifications

Extra clarity that helps in practice:

1) Linking & relocations (hash → handle)

On-disk cells use CIDs for callees and blobs.

At load you build a map cid → Word* (or small index) and rewrite each XT_REF to XT(Word*).

Keep a tiny reloc log only if you want to support hot-relink (e.g., switching “root” without rebuilding the in-memory image). Otherwise, once linked, the in-RAM code stream is final.


2) Blob headers vs “Value” on the stack

We decided no tags in stack slots.

For pointers, put size/kind in the object header the pointer references:
struct Blob { u32 kind; u32 flags; u64 len; u8 data[]; }.

If a primitive needs length/type often, it can cache them in locals; don’t bake caches into the stack format.


3) EXT form (future-proofing)

Keep fast tags at 2 bits (XT, EXIT, LIT_I64, EXT).

Under EXT, the next word is a small subtag describing how many words of payload follow. This lets you add LIT_F64, LIT_REF, SMALLSTR, DBG_MARK, CALL_INDIRECT, etc., later—without touching the fast path.

4) Control-flow compilation (backpatching)

IF … ELSE … THEN (but different syntax): during compile, emit conditional-jump cells with a placeholder target (e.g., store the future IP offset as 0 for now). Keep a short stack of “patch sites.”

At ELSE/THEN, compute the current code length and backpatch the earlier cell’s payload (in canonical bytes).

When storing to DB, you’re storing already backpatched, canonical code (reproducible hash).


5) Manifests, GC, and dead code

Module manifest (hash) lists exported word hashes and dependencies.

Roots table says which manifest(s) are live.

GC sweep: BFS/DFS from roots following references parsed out of code and descriptors (or precomputed edge rows). Delete unmarked objects.

Cheap and safe because objects are immutable.


6) Bytecode versioning

Put bytecode_version in every code object.

On loader mismatch, refuse to link (or trigger recompile).

Schema upgrades are painless because you never mutate old objects.

(A little scared of this idea though.)


7) FFI ABI (for SQLite & friends)

Keep it boringly simple:

Calling convention: pass a pointer to VM and use the VM’s data stack for args/results (pop N, push M).

Value-level ABI: stack slot is a word; pointers are to your blobs/records; ints are 64-bit.

Ownership: FFI allocators return pointers into workspace heap unless explicitly frozen.

Error: return a small status code; push an error blob only if needed.


8) “Auto mutability” lowering (borrow/uniqueness)

Lattice: U (unique), B (borrowed/shared), E (escapes).

Lower pure updates to in-place when U and non-escaping; insert freeze exactly at store/return boundaries (or when uniqueness is lost).

Start intraprocedural; summaries (purity/escape) are a bonus.

(User can still optionally force one or the other.)


9) Peepholes worth doing immediately

Tail-EXIT fuse: if a colon word ends … XT EXIT, turn that into a tail call to XT (pop return + jump to callee).

LIT_I64 small: if top-of-stack consumer is an immediate arithmetic primitive, you can sometimes fuse the literal load into that primitive (optional; measure first).

