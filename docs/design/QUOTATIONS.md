# Quotation Design

THIS DOCUMENT IS PARTIALLY OUT OF DATE.

WE FOUND A WAY TO HANDLE QUOTES WITHOUT HAVING TO TYPE THEM -- treat them just like words, which have to be rarfified by the JIT.

## Overview

March quotations are essentially thunks, but they are not closures.
Thus March quotes compile just like words.

## Syntax

Quotations are (generally) notated with parenthesies `( ... )`.

- **Lexical quotations**: `( 42 )` - compile-time only, must be consumed in current scope
- **Typed quotations**: `( _i64 42 )` - can be passed, returned, and stored

### Complete Quotations

```march
( body )
```

**Properties:**
- Compile-time values on the quotation stack
- Can be consumed by an immediate words (`if`, `map`, `each`, etc.)
- If immediate word's arguments are *complete* (within the scope of a word), then:
  - **Zero runtime cost** - completely inlined
- No type markers needed.

**Examples:**
```march
: choose
    condition
    ( handle-true )
    ( handle-false )
    if                   -- Consumes both
;

: process-array
    my-array ( dup * ) map   -- Lexical, inlined into loop
;
```

### Incomplete Quotations

Quotations that flow through word boundries, as inputs and outputs,
are *compiled* and *called* by the runtime just like words,
only they are annonymous words.

```march
: ifitisso ( body ) if ;
```

The above example needs a boolean and another quotation to complete it's expected parameters,
so it can not inline the quotation.

Quotations can be made concrete by the programmer by providing type signatures, notated with underscores.
There allow the quotes to fully lowered by the compiler regardless of how it used.

```march
( _type1 _type2 ... body )
```

**Properties:**
- Can be passed to other words
- Can be returned from words
- Can be stored in data structures
- Can be called with `execute`
- Input types declared with `_type` markers
- Output types automatically inferred from body
- Possibly can still be inlined sometimes by immediate words (optimization)

**Examples of typed quotes:**
```march
: make-doubler
    ( _i64 2 * )         -- Returns typed quotation
;                        -- Output type inferred: i64

: operations
    [
        ( _i64 abs )     -- Can store in array
        ( _i64 negate )
        ( _i64 _i64 + )  -- Multiple inputs
    ]
;
```

## Type Markers

Type markers use underscore prefix: `_typename`

**Concrete types:**
- `_i64` - 64-bit signed integer
- `_f64` - 64-bit float
- `_bool` - Boolean
- `_ptr` - Raw pointer
- `_str` - String (future)

**Type variables (future):**
- `_a`, `_b`, `_c` - Polymorphic type variables
- Requires runtime specialization/JIT (just like words).

**How they work:**
1. Declare expected input types in order
2. Compiler validates body against these types
3. Output types inferred from body compilation
4. Flag quotation as `QUOT_TYPED` (can escape scope unincumbered)

## Immediate Words and Inlining

Immediate words like `if`, `map`, `each` can **inline** quotation bodies for performance.

### Inlining Rules

**Lexical (Complete) quotations:**
- MUST be inlined (only option - no runtime representation)
- Zero overhead
- Body compiled directly into parent word

**Unclosed quotations:**
- no inlining, compiled as annonymous words
- CALLED like words

**Typed quotations:**
- POSSIBLY CAN be inlined (optimization)
- Immediate word decides based on:
  - Concrete vs polymorphic types
  - Body complexity
  - Optimization level

### Example: `map` with inlining

```march
: double-all
    array ( 2 * ) map
;
```

Typed, but still inlined.

```march
: double-all-i64
    array ( _i64 2 * ) map
;
```

Even though `( _i64 2 * )` is typed (could be passed around), `map` can inline it:

```c
// Generated code (conceptual):
for (i = 0; i < array->length; i++) {
    element = array[i];    // i64
    result = element * 2;  // Inlined body, specialized to i64!
    array[i] = result;
}
```

No quotation object created, no indirect call overhead!

## Compilation Model

Not 100% sure about the following model -- very rough sketch.

### Regular Quotation Flow

```
1. `(` → Create quotation, push to quot_stack, switch buffers
2. tokens → Compile into quotation's buffers
3. `)` → Mark as QUOT_WORD, restore parent buffers
4. `;` -> Compile quotations on quote stack as if words.
```

### Lexical Quotation Flow

```
1. `(` → Create quotation, push to quot_stack, switch buffers
2. tokens → Compile into quotation's buffers
3. `)` → Mark as QUOT_LEXICAL, restore parent buffers
4. Immediate word → Pop from quot_stack, inline body
```

### Typed Quotation Flow

```
1. `(` → Create quotation, push to quot_stack, switch buffers
2. `_type` → Record input type, push to type stack
3. tokens → Compile with known input types
4. `)` → Infer output types, mark as QUOT_TYPED, materialize to blob/CID
5. Immediate word → Can inline OR emit as runtime value
6. `;` → Typed quotations OK to remain (become runtime values)
```

## Type Inference

Output types are inferred by tracking the type stack during quotation body compilation:

```march
( _i64 _i64 + )
```

**Inference:**
```
Start:   type_stack = []
_i64  →  type_stack = [i64]
_i64  →  type_stack = [i64, i64]
+     →  Lookup + with [i64, i64] → finds op_add_i64
         op_add_i64 signature: i64 i64 -> i64
         Apply: pop i64, pop i64, push i64
         type_stack = [i64]
End:     outputs = [i64]
```

**Result:** `( _i64 _i64 + )` has signature `i64 i64 -> i64`

## Error Cases

### Incomplete Quotation

```march
: bad
    ( 42               -- ERROR: quotation not finished
;
```

```march
: bad
    42 )               -- ERROR: quotation not started
;
```

### Missing Type Markers for Passable Quotation

```march
: return-quot
    ( 42 )               -- compile like a word, essentially an XT is returned
;
```

Or explicitly:
```march
: return-quot
    ( _i64 )             -- Pushes input, returns it (identity function)
;
```

## Performance Model

| Pattern | Runtime Cost | Use Case |
|---------|--------------|----------|
| `( body ) if` | Zero | Inlined conditionals |
| `( body ) map` | Zero | Inlined loop bodies |
| `( _i64 body )` inlined | Zero | Typed but inlined by smart immediate word |
| `( _i64 body )` called | Function call | Stored in data structure, late-bound |

**Rule of thumb:** If the quotation is consumed immediately by an immediate word, it's free!

## Future: Polymorphic Quotations

Currently deferred, but possible approaches:

### Option 1: Type Variables
```march
( _a _a )                -- Generic identity: a -> a
```

Requires runtime specialization/JIT.

### Option 2: Explicit Instantiation
```march
: identity ( _a _a ) ;
identity @i64            -- Instantiate for i64
identity @f64            -- Instantiate for f64
```

### Option 3: Inference at Use Site
```march
5 ( _a _a ) execute      -- Infer a=i64 from stack
```

Specialize on first call, cache result.

## Design Rationale

**Why lexical by default?**
- Most quotations are consumed immediately (`if`, `map`, `each`)
- Zero-cost abstraction principle
- Performance is predictable
- Explicit when you pay for runtime flexibility

**Why `_type` markers (not separate declaration)?**
- Self-documenting: see types where they're used
- Pattern-like syntax: reads naturally
- No redundant syntax (`$( )` vs `( _type )`)
- Dual purpose: documentation + capability flag

**Why infer output types?**
- Less verbose
- Matches FORTH tradition (stack effects)
- Compiler already tracks types during compilation
- Catches errors automatically

**Why allow inlining of typed quotations?**
- Performance: don't pay for flexibility you don't use
- Optimization: immediate words can choose fast path
- Gradual: start with inlining, fall back to calls if needed

## Examples

### Common Patterns

**Conditionals:**
```march
: abs
    dup 0 <
    ( negate )           -- Lexical, inlined
    ( )                  -- Lexical, inlined (empty = identity)
    if
;
```

**Mapping:**
```march
: double-all
    ( _i64 2 * ) map     -- Typed (can be inlined or called)
;
```

**Higher-order functions:**
```march
: apply-twice ( value ( _i64 -- _i64 ) -- result )
    swap dup >r          -- ( value value ) r:( quot )
    execute              -- ( value result ) r:( quot )
    r> execute           -- ( result )
;

5 ( _i64 2 * ) apply-twice   -- Result: 20
```

**Storing operations:**
```march
: math-ops
    [
        ( _i64 _i64 + )
        ( _i64 _i64 - )
        ( _i64 _i64 * )
    ]
;
```

## Implementation Status

- [x] Lexical quotations for `if`
- [x] Execute primitive
- [ ] Type marker parsing (`_type`)
- [ ] QUOT_LEXICAL vs QUOT_TYPED distinction
- [ ] Scope checking (error on unconsumed lexical)
- [ ] Output type inference
- [ ] `map`/`each` with inlining
- [ ] Polymorphic quotations (future)
