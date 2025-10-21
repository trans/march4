# March Language - CID-Based Storage and Runtime Linking

## Overview

March uses content-addressable storage where every blob is identified by its SHA256 hash (CID). This design document describes how compiled code is stored as CID sequences and how the loader transforms these into executable machine code at runtime.

### The Content-Addressable Requirement

**Core principle**: Identical source code must produce identical CIDs, regardless of when or where it's compiled.

**Why this matters**:
- Enables true content-addressable caching
- Makes builds reproducible
- Allows sharing of compiled code across machines
- Supports incremental compilation (only recompile what changed)

**What breaks content-addressing**:
- Storing runtime memory addresses/pointers in blobs
- Timestamps or system-specific metadata
- Non-deterministic compilation order

### The Pipeline

```
Source Code
    ↓
[COMPILER] - Type checking, CID emission
    ↓
CID Sequences (stored in database)
    ↓
[LOADER] - CID resolution and linking
    ↓
Runtime Cells (XT/LIT with actual addresses)
    ↓
[VM] - Execution
```

The compiler never sees runtime addresses. The loader never sees source code. Each stage works with appropriate abstractions.

---

## Storage Format

### Blob Structure

A compiled word is stored as a **variable-length encoded sequence**:

```
Blob data = tag₁ [CID₁] || tag₂ [CID₂] || ... || tagₙ [CIDₙ]
            └────┬────┘
         1-2 bytes + optional 32-byte CID
```

Each element is either:
- **Primitive**: 1-2 byte tag (no CID)
- **Content reference**: 1-2 byte tag + 32-byte CID

Example: `: double dup + ;`
```
Blob contains:
  0x0A        (primitive #5 = dup, 1 byte)
  0x02        (primitive #1 = add, 1 byte)
Total: 2 bytes!
```

Example: `: fifteen 5 10 + ;`
```
Blob contains:
  0x07 + [32-byte CID]  (BLOB_DATA, literal 5)
  0x07 + [32-byte CID]  (BLOB_DATA, literal 10)
  0x02                  (primitive #1 = add)
Total: 67 bytes (vs 96 bytes with fixed 32-byte encoding)
```

### Variable-Length Tag Encoding

March uses **UTF-8 style variable-length encoding** for tags:

**Format** (little-endian):
```
First byte:
  Bit 7: continuation (1 = more bytes, 0 = done)
  Bits 0-6: data

Each continuation byte:
  Bit 7: continuation
  Bits 0-6: data (accumulated left-to-right)
```

**1-byte values** (0-127):
```
0XXXXXXX
└─ 7 data bits
```

**2-byte values** (128-16383):
```
1XXXXXXX 0XXXXXXX
└─ 7 bits  └─ 7 bits (total: 14 data bits)
```

**Interpreting the data value**:
```
Data bit 0:
  0 = Primitive (remaining bits = primitive ID)
  1 = CID follows (remaining bits = blob kind)

Examples:
  Value 10 = 0b00001010 → bit0=0, id=5 → Primitive #5
  Value 3  = 0b00000011 → bit0=1, kind=1 → BLOB_WORD CID follows
```

**Why this format?**
- Primitives are 1-2 bytes (not 32!)
- ~32x space savings for primitive-heavy code
- Still supports full 32-byte CIDs for content-addressing
- Fast decoding (simple loop)
- Little-endian (efficient on x86-64/ARM)

### Encoding and Decoding Algorithms

**Decoding** (loader reads blob):
```c
uint8_t* decode_tag(uint8_t* ptr, bool* is_cid, uint16_t* id_or_kind, char** cid) {
    // Decode variable-length value
    uint32_t value = 0;
    int shift = 0;
    uint8_t byte;

    do {
        byte = *ptr++;
        value |= (byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    // Interpret data bit 0
    if (value & 1) {
        // CID follows
        *is_cid = true;
        *id_or_kind = value >> 1;  // Blob kind
        *cid = (char*)ptr;
        return ptr + 32;  // Skip CID
    } else {
        // Primitive
        *is_cid = false;
        *id_or_kind = value >> 1;  // Primitive ID
        *cid = NULL;
        return ptr;
    }
}
```

**Encoding** (compiler writes blob):
```c
// Encode primitive
void encode_primitive(buffer_t* buf, uint16_t prim_id) {
    uint32_t value = (prim_id << 1) | 0;  // Bit 0 = 0

    while (value >= 0x80) {
        buffer_append(buf, (value & 0x7F) | 0x80);
        value >>= 7;
    }
    buffer_append(buf, value & 0x7F);
}

// Encode CID reference
void encode_cid_ref(buffer_t* buf, uint8_t kind, const char* cid) {
    uint32_t value = (kind << 1) | 1;  // Bit 0 = 1

    while (value >= 0x80) {
        buffer_append(buf, (value & 0x7F) | 0x80);
        value >>= 7;
    }
    buffer_append(buf, value & 0x7F);

    // Append 32-byte CID
    buffer_append_bytes(buf, cid, 32);
}
```

**Encoding examples**:
```c
// Primitive #5 (dup)
// value = (5 << 1) | 0 = 10 = 0b00001010
// Result: 0x0A (1 byte)

// Primitive #200
// value = (200 << 1) | 0 = 400 = 0b0000000110010000
// Byte 1: 0b10010000 = 0x90 (continue=1, data=0010000)
// Byte 2: 0b00000011 = 0x03 (continue=0, data=0000011)
// Result: 0x90 0x03 (2 bytes)

// BLOB_WORD CID
// value = (1 << 1) | 1 = 3 = 0b00000011
// Result: 0x03 + 32-byte CID (33 bytes total)
```

### No Runtime Addresses in Storage

Storage blobs contain **only CIDs**. No:
- Function pointers
- Memory addresses
- Process-specific values

This ensures CIDs remain stable across compilations and machines.

---

## CID Computation by Blob Kind

### Primitives (BLOB_PRIMITIVE)

**Source**: Assembly files in `kernel/x86-64/*.asm`

**CID Assignment**: Primitives use **fixed IDs** that never change:

```c
// primitives.h - Fixed primitive ID table
#define PRIM_ADD     1
#define PRIM_SUB     2
#define PRIM_MUL     3
#define PRIM_DUP     4
#define PRIM_DROP    5
// ... etc (up to 255)

// Convert primitive ID to CID
char* primitive_cid(uint8_t prim_id) {
    char* cid = calloc(32, 1);  // All zeros
    cid[31] = prim_id;          // Set last byte to ID
    // Bit 0 is 0 (primitive marker)
    return cid;
}
```

**Why fixed IDs?**
- Assembly implementations can change (optimizations, bug fixes)
- Compiled code must remain valid
- Primitives are the stable foundation

**Storage**:
- `blobs` table: CID=primitive_cid(ID), kind=BLOB_PRIMITIVE, data=assembly source (optional, for reference)
- `words` table: name="+" → def_cid=primitive_cid(PRIM_ADD)
- Dictionary: name → primitive CID

**Example**:
```
Primitive "add":
  ID = 1
  CID = 0x0000000000000000000000000000000000000000000000000000000000000001
  Assembly: kernel/x86-64/add.asm (stored for reference, not hashed)
```

### User Words (BLOB_WORD)

**CID Computation**:
```
word_blob = encoded_sequence (variable-length)
CID = SHA256(word_blob)
```

The word's CID is the hash of its variable-length encoded blob. EXIT is not stored - the linker appends it.

**Example**: `: double dup + ;`
```
word_blob = 0x0A 0x02  (2 bytes: dup primitive, add primitive)
CID_double = SHA256(0x0A 0x02)
```

**Note**: Because we hash the encoded sequence (not source text), different source producing the same code gets the same CID (good! - deduplication). Primitives are encoded by ID, so changing assembly doesn't change CIDs.

### Quotations (BLOB_QUOTATION)

**CID Computation**: Same as user words
```
quot_blob = encoded_sequence (variable-length)
CID = SHA256(quot_blob)
```

**Difference from words**:
- No entry in `words` table (anonymous)
- `kind = BLOB_QUOTATION` (tells loader to push address, not call)
- EXIT appended by linker (or omitted when inlining)

**Example**: `[ 10 + ]`
```
quot_blob = 0x07 [CID_lit10] 0x02  (tag + CID for literal, tag for add)
CID_quot = SHA256(quot_blob)
```

### Literals (BLOB_DATA)

**CID Computation**:
```
data_bytes = little_endian_encoding(value)
CID = SHA256(data_bytes)
```

**Storage**:
- `blobs` table: data = raw bytes
- `sig_cid` → type signature (e.g., "->i64")

**Example**: Literal `42`
```
data_bytes = [0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]  // 42 as i64
CID_42 = SHA256(data_bytes)
```

**For other types**:
- Strings: UTF-8 bytes, sig_cid → "->string"
- Arrays: serialized elements, sig_cid → "->[i64]"
- Symbols: via `symbols` table (existing mechanism)

---

## Primitive Registration

### Current (Broken) Approach
```c
// primitives.c - WRONG!
dict_add(dict, "+", &op_add, ...);  // Stores runtime pointer
```

### Correct Approach

**At compiler startup**:

1. **Compute primitive CIDs** (one-time, could be cached):
```c
char* cid_add = compute_primitive_cid("kernel/x86-64/add.asm");
char* cid_dup = compute_primitive_cid("kernel/x86-64/dup.asm");
// ... for all primitives
```

2. **Store primitive blobs** (if not already in DB):
```c
db_store_primitive(db, cid_add, asm_source, &op_add);
// Stores: CID, kind=BLOB_PRIMITIVE, data=asm_source
// Associates runtime address for this process
```

3. **Register in dictionary**:
```c
dict_add(dict, "+", cid_add, sig, ...);
// Dictionary stores CID, not address
```

**At link time**:
- Loader sees CID_add in blob
- Looks up primitive: finds runtime address `&op_add`
- Emits `[XT &op_add]` in runtime cell buffer

**Note**: Primitive addresses are still process-specific, but they're resolved at load time, not stored in blobs.

---

## Linking Algorithm

### Loader Initialization

```c
typedef struct {
    march_db_t* db;
    dictionary_t* dict;

    // Cache of loaded code
    hashtable_t* cid_to_addr;  // CID → runtime address

    // Memory management
    list_t* allocated_buffers;  // Track for cleanup
} loader_t;
```

### Loading a Word

```c
void* loader_load_word(loader_t* loader, const char* name) {
    // 1. Look up word's CID in dictionary
    char* word_cid = dict_lookup_cid(loader->dict, name);

    // 2. Link the CID (recursive)
    return loader_link_cid(loader, word_cid);
}
```

### The Core Linking Algorithm

```c
void* loader_link_cid(loader_t* loader, const char* cid) {
    // Check cache first
    void* cached = hashtable_get(loader->cid_to_addr, cid);
    if (cached) return cached;

    // Load blob metadata
    int kind;
    char* sig_cid;
    uint8_t* blob_data;
    size_t blob_len;
    db_load_blob(loader->db, cid, &kind, &sig_cid, &blob_data, &blob_len);

    void* result;

    switch (kind) {
        case BLOB_PRIMITIVE:
            // Look up primitive's runtime address
            result = dict_get_primitive_addr(loader->dict, cid);
            break;

        case BLOB_WORD:
        case BLOB_QUOTATION:
            // Recursively link
            result = loader_link_code(loader, blob_data, blob_len, kind);
            break;

        case BLOB_DATA:
            // Literal value - just allocate and store
            result = malloc(blob_len);
            memcpy(result, blob_data, blob_len);
            break;
    }

    // Cache result
    hashtable_put(loader->cid_to_addr, cid, result);

    return result;
}
```

### Linking Code Blobs

```c
void* loader_link_code(loader_t* loader, uint8_t* blob_data, size_t blob_len) {
    // Allocate runtime cell buffer (estimate size, realloc if needed)
    size_t capacity = 64;
    size_t count = 0;
    uint64_t* cells = malloc(capacity * sizeof(uint64_t));

    uint8_t* ptr = blob_data;
    uint8_t* end = blob_data + blob_len;

    while (ptr < end) {
        bool is_cid;
        uint16_t id_or_kind;
        char* cid;

        // Decode next tag
        ptr = decode_tag(ptr, &is_cid, &id_or_kind, &cid);

        // Expand buffer if needed
        if (count >= capacity) {
            capacity *= 2;
            cells = realloc(cells, capacity * sizeof(uint64_t));
        }

        if (!is_cid) {
            // Primitive: look up runtime address by ID
            void* prim_addr = get_primitive_addr(loader, id_or_kind);
            cells[count++] = encode_xt(prim_addr);
        } else {
            // CID reference: recursively link
            void* addr = loader_link_cid(loader, cid);

            switch (id_or_kind) {  // id_or_kind is blob kind here
                case BLOB_WORD:
                    // Call it
                    cells[count++] = encode_xt(addr);
                    break;

                case BLOB_QUOTATION:
                    // Push its address
                    cells[count++] = encode_lit((int64_t)addr);
                    break;

                case BLOB_DATA:
                    // Push the value
                    int64_t value = *(int64_t*)addr;
                    cells[count++] = encode_lit(value);
                    break;
            }
        }
    }

    // Append EXIT
    cells[count++] = encode_exit();

    return cells;
}
```

### The Key Insight

The **blob's kind** determines how it's used when referenced:
- `BLOB_WORD/BLOB_PRIMITIVE` → emit `[XT addr]` (call it)
- `BLOB_QUOTATION` → emit `[LIT addr]` (push it)
- `BLOB_DATA` → emit `[LIT value]` (push value)

No operation tags needed in the CID sequence! The kind field in the metadata provides all the information.

---

## Type Signatures in Linking

### Compile-Time Role (Primary)

Type signatures are **primarily** for compile-time type checking:
- Ensure stack effects match
- Resolve overloads
- Catch errors before code runs

### Link-Time Role (Secondary)

Type signatures **can** be used at link time for:

1. **Verification** (optional):
```c
// Verify that quotation's signature matches what's expected
if (debug_mode) {
    verify_signature_match(expected_sig, actual_sig);
}
```

2. **Optimization hints** (future):
```c
// If pure + no side effects, could inline aggressively
if (is_pure(sig_cid)) {
    consider_inlining(cid);
}
```

3. **Debugging**:
```c
// Show human-readable info
fprintf(stderr, "Linking: %s with sig: %s\n",
        cid, lookup_signature_text(sig_cid));
```

### Not Required for Correctness

The loader can link code **without** examining type signatures. The kind field provides all necessary information. Types are for compile-time safety and developer clarity.

---

## Complete Examples

### Example 1: Simple Literal Word

**Source**: `: five 5 ;`

**Compilation**:
1. See literal `5`
2. Create blob: `data=[5 as i64]`, `kind=BLOB_DATA`, `sig_cid=SHA256("->i64")`
3. Compute `CID_5 = SHA256(data)`
4. Encode: `0x07` (kind=BLOB_DATA=3, value=(3<<1)|1=7) + `CID_5`
5. Compute `CID_five = SHA256(0x07 || CID_5)`
6. Store word blob, create words entry

**Storage** (blobs table):
```
cid=CID_5, kind=BLOB_DATA, sig_cid=sig_i64, data=[0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
cid=CID_five, kind=BLOB_WORD, sig_cid=sig_to_i64, data=[0x07, CID_5...]
     └─ 33 bytes: tag + 32-byte CID
```

**Storage** (words table):
```
name="five", def_cid=CID_five, type_sig="->i64"
```

**Linking**:
1. Load `CID_five` blob → `[0x07, CID_5...]`
2. Decode tag: is_cid=true, kind=BLOB_DATA, cid=CID_5
3. Link `CID_5`: kind=BLOB_DATA → load data → value=5
4. Emit `[LIT 5]`
5. Append `[EXIT]`
6. Result: `[LIT 5] [EXIT]` at runtime address X

**Execution**:
```
VM jumps to address X
Executes [LIT 5] → pushes 5
Executes [EXIT] → returns
Stack: [5]
```

### Example 2: Primitive Call

**Source**: `: double dup + ;`

**Compilation**:
1. Look up `dup` → primitive #5
2. Look up `+` → primitive #1
3. Encode: `0x0A` (prim 5: (5<<1)|0=10) `0x02` (prim 1: (1<<1)|0=2)
4. Compute `CID_double = SHA256(0x0A 0x02)`

**Storage** (blobs table):
```
cid=CID_double, kind=BLOB_WORD, sig_cid=sig_i64_to_i64, data=[0x0A, 0x02]
     └─ Just 2 bytes!
```

**Linking**:
1. Load `CID_double` blob → `[0x0A, 0x02]`
2. Decode tag `0x0A`: is_cid=false, prim_id=5 → lookup `&op_dup`
3. Emit `[XT &op_dup]`
4. Decode tag `0x02`: is_cid=false, prim_id=1 → lookup `&op_add`
5. Emit `[XT &op_add]`
6. Append `[EXIT]`
7. Result: `[XT &op_dup] [XT &op_add] [EXIT]` at address Y

**Execution**:
```
Stack before: [10]
VM jumps to address Y
Executes [XT &op_dup] → calls op_dup → stack: [10, 10]
Executes [XT &op_add] → calls op_add → stack: [20]
Executes [EXIT] → returns
Stack after: [20]
```

**Space savings**: 2 bytes vs 64 bytes (32x smaller!)

### Example 3: Quotation

**Source**: `: make-adder [ 10 + ] ;`

**Compilation**:
1. Enter quotation `[`
2. Compile quotation body:
   - Literal `10` → create BLOB_DATA, get CID_10
   - Encode: `0x07` + CID_10 (BLOB_DATA tag + CID)
   - Primitive `+` → encode: `0x02` (primitive #1)
3. Close quotation `]`:
   - Build: `0x07 [CID_10] 0x02`
   - Compute `CID_quot = SHA256(0x07 || CID_10 || 0x02)`
   - Store blob with `kind=BLOB_QUOTATION`
4. In parent word, encode: `0x05` + CID_quot (BLOB_QUOTATION=2, (2<<1)|1=5)
5. Compute `CID_make_adder = SHA256(0x05 || CID_quot)`

**Storage** (blobs table):
```
cid=CID_10, kind=BLOB_DATA, data=[10 as i64]
cid=CID_quot, kind=BLOB_QUOTATION, sig_cid=sig_i64_to_i64,
              data=[0x07, CID_10..., 0x02]  (34 bytes)
cid=CID_make_adder, kind=BLOB_WORD, sig_cid=sig_to_ptr,
                    data=[0x05, CID_quot...]  (33 bytes)
```

**Linking** `make-adder`:
1. Load `CID_make_adder` → `[0x05, CID_quot...]`
2. Decode tag `0x05`: is_cid=true, kind=BLOB_QUOTATION, cid=CID_quot
3. Link `CID_quot`:
   - Load quotation blob: `[0x07, CID_10..., 0x02]`
   - Decode `0x07`: kind=BLOB_DATA, load literal 10 → emit `[LIT 10]`
   - Decode `0x02`: primitive #1 → emit `[XT &op_add]`
   - Append `[EXIT]`
   - Result: quotation cells at address Q
4. Because kind==BLOB_QUOTATION, emit `[LIT Q]` (push address!)
5. Append `[EXIT]`
6. Result: `[LIT Q] [EXIT]` at address M

**Execution** of `make-adder`:
```
VM jumps to M
Executes [LIT Q] → pushes address Q
Executes [EXIT] → returns
Stack: [Q]
```

**Execution** of `Q execute`:
```
Stack: [Q, 5]
execute primitive:
  Pops Q from stack
  Calls address Q
VM jumps to Q:
  Executes [LIT 10] → stack: [5, 10]
  Executes [XT &op_add] → stack: [15]
  Executes [EXIT] → returns to execute
execute returns
Stack: [15]
```

### Example 4: Word Calling Word

**Source**:
```
: five 5 ;
: fifteen five 10 + ;
```

**Compilation** of `fifteen`:
1. Look up `five` → `CID_five` (user word, already compiled)
2. Encode: `0x03` + CID_five (BLOB_WORD=1, (1<<1)|1=3)
3. Literal `10` → `CID_10`, encode: `0x07` + CID_10
4. Primitive `+` → encode: `0x02`
5. Build: `0x03 [CID_five] 0x07 [CID_10] 0x02`
6. `CID_fifteen = SHA256(...)`

**Storage** (blobs table):
```
cid=CID_fifteen, kind=BLOB_WORD, data=[0x03, CID_five..., 0x07, CID_10..., 0x02]
     └─ 67 bytes: (1+32) + (1+32) + 1
```

**Linking** `fifteen`:
1. Load blob: `[0x03, CID_five..., 0x07, CID_10..., 0x02]`
2. Decode `0x03`: kind=BLOB_WORD, cid=CID_five
   - Recursively link five's blob → address F
   - Emit `[XT F]` (call it!)
3. Decode `0x07`: kind=BLOB_DATA, cid=CID_10
   - Load literal 10
   - Emit `[LIT 10]`
4. Decode `0x02`: primitive #1
   - Emit `[XT &op_add]`
5. Append `[EXIT]`
6. Result: `[XT F] [LIT 10] [XT &op_add] [EXIT]`

**Execution**:
```
VM executes [XT F]:
  Calls address F (five's code)
  Returns with stack: [5]
VM executes [LIT 10]:
  Stack: [5, 10]
VM executes [XT &op_add]:
  Stack: [15]
```

---

## Implementation Changes Needed

### 1. Types (types.h)

**Add**:
```c
#define BLOB_QUOTATION  2    /* Executable code (push address, not call) */
#define BLOB_PRIMITIVE  0    /* Assembly primitive (optional flag, not stored) */

// Primitive ID table
#define PRIM_ADD        1
#define PRIM_SUB        2
#define PRIM_MUL        3
#define PRIM_DIV        4
#define PRIM_DUP        5
#define PRIM_DROP       6
#define PRIM_SWAP       7
// ... (up to 255)
```

**Change**:
```c
// Blob buffer - variable-length encoded
typedef struct {
    uint8_t* data;     // Raw bytes
    size_t size;       // Current size
    size_t capacity;   // Allocated capacity
} blob_buffer_t;

// Encoding functions
void encode_primitive(blob_buffer_t* buf, uint16_t prim_id);
void encode_cid_ref(blob_buffer_t* buf, uint8_t kind, const char* cid);
```

### 2. Compiler (compiler.c)

**Major changes**:
```c
// OLD: cell_buffer_append(comp->cells, encode_xt(entry->addr));
// NEW: encode into blob_buffer

// For literals:
char* cid = db_store_literal(comp->db, value, type);
encode_cid_ref(comp->blob, BLOB_DATA, cid);  // 1 byte + 32-byte CID

// For primitives:
dict_entry_t* entry = dict_lookup(comp->dict, "+");
encode_primitive(comp->blob, entry->prim_id);  // 1-2 bytes only!

// For user words:
dict_entry_t* entry = dict_lookup(comp->dict, "five");
encode_cid_ref(comp->blob, BLOB_WORD, entry->cid);  // 1 byte + 32-byte CID

// For quotations:
char* quot_cid = materialize_quotation(comp, quot);
encode_cid_ref(comp->blob, BLOB_QUOTATION, quot_cid);  // 1 byte + 32-byte CID

// EXIT: NOT emitted! Linker adds based on blob kind

// At end of word definition:
char* word_cid = compute_sha256(comp->blob->data, comp->blob->size);
db_store_word(comp->db, name, namespace, comp->blob->data, comp->blob->size, type_sig, source);
```

### 3. Database (database.c/h)

**Add functions**:
```c
// Store literal, return CID
char* db_store_literal(march_db_t* db, int64_t value, type_id_t type);

// Load any blob by CID
bool db_load_blob(march_db_t* db, const char* cid,
                  int* kind, char** sig_cid,
                  uint8_t** data, size_t* len);

// Get just the kind (for fast lookup)
int db_get_blob_kind(march_db_t* db, const char* cid);

// Store primitive blob
bool db_store_primitive(march_db_t* db, const char* cid,
                        const char* asm_source, void* runtime_addr);
```

### 4. Primitives (primitives.c)

**Change registration**:
```c
void register_primitives(dictionary_t* dict, march_db_t* db) {
    type_sig_t sig;

    // Use fixed primitive IDs (from primitives.h)
    char* cid_add = primitive_cid(PRIM_ADD);  // 0x00...0001
    char* cid_dup = primitive_cid(PRIM_DUP);  // 0x00...0004

    // Store primitive blob (associates CID with runtime address)
    // Assembly source stored for documentation/reference only
    db_store_primitive(db, cid_add, "add.asm", &op_add);
    db_store_primitive(db, cid_dup, "dup.asm", &op_dup);

    // Register in dictionary with CID
    parse_type_sig("i64 i64 -> i64", &sig);
    dict_add(dict, "+", cid_add, &sig, true, false, NULL);

    parse_type_sig("i64 -> i64 i64", &sig);
    dict_add(dict, "dup", cid_dup, &sig, true, false, NULL);

    // ... repeat for all primitives
}

// Helper function
char* primitive_cid(uint8_t prim_id) {
    char* cid = calloc(32, 1);  // All zeros
    cid[31] = prim_id;          // Set last byte to ID
    return cid;                 // Bit 0 is 0 (primitive)
}

bool is_primitive_cid(const char* cid) {
    // Check if first bit is 0
    return (cid[0] & 0x80) == 0;
}
```

### 5. Dictionary (dictionary.h/c)

**Change entry structure**:
```c
typedef struct dict_entry {
    char* name;

    // For primitives:
    uint16_t prim_id;       // Primitive ID (1-255)
    void* addr;             // Runtime address

    // For user words:
    char* cid;              // CID of the word's blob

    type_sig_t signature;
    bool is_primitive;
    bool is_immediate;
    immediate_handler_t handler;
    struct dict_entry* next;
} dict_entry_t;
```

**Lookup functions**:
```c
dict_entry_t* dict_lookup(dictionary_t* dict, const char* name);

// For linker:
void* get_primitive_addr_by_id(dictionary_t* dict, uint16_t prim_id);
```

### 6. Loader (loader.c)

**Complete rewrite**:
```c
typedef struct {
    march_db_t* db;
    dictionary_t* dict;

    // CID → runtime address cache
    hashtable_t* cid_to_addr;

    // Track allocations for cleanup
    list_t* allocated_buffers;
} loader_t;

// Main linking functions
void* loader_link_cid(loader_t* loader, const char* cid);
void* loader_link_code(loader_t* loader, const char* cid_seq, size_t len);
void* loader_load_word(loader_t* loader, const char* name);
```

### 7. Tests

**Update all tests**:
- Verify CID stability (same source → same CID)
- Test literal compilation
- Test primitive calls
- Test user word calls
- Test quotations
- Test nested quotations

**New tests**:
- `test_cid_stability.c` - Compile twice, compare CIDs
- `test_literal_cids.c` - Same literal gets same CID
- `test_linking.c` - Full pipeline test

---

## Migration Strategy

### Phase 1: Parallel Implementation
- Keep current system working
- Implement CID-based storage alongside
- Add compiler flag: `--cid-mode`
- Tests can run in both modes

### Phase 2: Gradual Migration
- Convert tests one by one to CID mode
- Fix issues as they arise
- Verify CID stability

### Phase 3: Verification
- Ensure all 215+ tests pass in CID mode
- Verify same source → same CID
- Test cross-compilation scenarios

### Phase 4: Switch
- Make CID mode the default
- Remove old pointer-based code
- Update documentation

---

## Space Savings Analysis

### Encoding Sizes

**Fixed 32-byte CID encoding** (original design):
- Primitive: 32 bytes
- User word: 32 bytes
- Quotation: 32 bytes
- Literal: 32 bytes

**Variable-length encoding** (new design):
- Primitive: 1-2 bytes (typically 1)
- User word: 33 bytes (1-byte tag + 32-byte CID)
- Quotation: 33 bytes (1-byte tag + 32-byte CID)
- Literal: 33 bytes (1-byte tag + 32-byte CID)

### Space Savings by Code Type

**Primitive-heavy code** (e.g., `: double dup + ;`):
- Old: 64 bytes (2 × 32)
- New: 2 bytes (1 + 1)
- **Savings: 97%** (32x smaller)

**Mixed code** (e.g., `: fifteen 5 10 + ;`):
- Old: 96 bytes (3 × 32)
- New: 67 bytes (33 + 33 + 1)
- **Savings: 30%**

**CID-heavy code** (many user word calls):
- Old: N × 32 bytes
- New: N × 33 bytes
- **Overhead: 3%** (acceptable for rare case)

### Real-World Estimates

Assuming typical code distribution:
- 50% primitives
- 30% literals
- 15% user word calls
- 5% quotations

**Average per reference**:
- Old: 32 bytes
- New: (0.5 × 1.5) + (0.3 × 33) + (0.15 × 33) + (0.05 × 33) = 17.25 bytes
- **Savings: ~46%**

For a 10,000-reference codebase:
- Old: 320 KB
- New: 172.5 KB
- **Saved: 147.5 KB**

### Benefits Beyond Size

1. **Faster primitive linking** - No database lookup, just array index
2. **Cache efficiency** - Smaller blobs fit better in CPU cache
3. **Faster transmission** - Less data to send over network
4. **Better compression** - Primitives are highly compressible (many duplicates)

---

## Open Questions

1. **Primitive CIDs**: ✅ RESOLVED - Use fixed ID table
   - Bit 0 = 0 for primitives
   - IDs 1-255 in fixed table (primitives.h)
   - Assembly can change without breaking compiled code

2. **String literals**: Encoding format?
   - UTF-8 bytes directly?
   - Length-prefixed?
   - Null-terminated?

3. **Array literals**: How to serialize?
   - Count + element CIDs?
   - Recursive blob structure?

4. **Caching**: Should we cache CID computations?
   - Keep map of source hash → CID?
   - Store in metadata table?

5. **Performance**: Is 32 bytes per CID too large?
   - Could use 16-byte truncated hashes (collision risk)
   - Could use CID indices (requires central registry)

---

## Summary

**Key Principles**:
1. **Variable-length encoding** - UTF-8 style, little-endian
2. **Primitives are 1-2 bytes** (not 32!) - massive space savings
3. Storage contains only encoded references, never runtime addresses
4. Identical code produces identical CIDs
5. **Data bit 0**: distinguishes primitives (0) from CID references (1)
6. **Primitives use fixed IDs** (stable forever, assembly can change)
7. **EXIT not stored** in blobs - linker appends based on kind
8. Blob kind determines linking behavior (call vs push)
9. Type signatures guide compilation, not linking
10. Linking is recursive and cached

**Benefits**:
- ✅ True content-addressable storage
- ✅ Reproducible builds
- ✅ **~50% space savings** on average (97% for primitive-heavy code)
- ✅ Faster primitive linking (no DB lookup)
- ✅ Better cache efficiency (smaller blobs)
- ✅ Shareable compiled code
- ✅ Incremental compilation
- ✅ Platform independence (for blobs)

**Tradeoffs**:
- More complex encoding/decoding (but simple algorithms)
- Variable-length parsing (but fast loop)
- Minimal overhead for CID references (1 byte vs 0 bytes)
