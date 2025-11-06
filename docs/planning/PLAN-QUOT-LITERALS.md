# QUOT_LITERAL Plan

## Progress

✅ IMPLEMENTATION COMPLETE!

### What's Done
- ✅ Type definitions added to `src/compiler.h`:
  - `quot_kind_t` enum (QUOT_LITERAL vs QUOT_TYPED)
  - Fields in `quotation_t`: `tokens`, `token_count`, `token_capacity`
- ✅ Polymorphic immediate word dispatch working
  - Type-aware lookup happens before checking is_immediate
  - `compile_times_dispatch` routes to correct handler based on quotation stack depth
- ✅ Token capture in compile_lparen/compile_rparen
  - Allocates `tokens` array with capacity 16
  - Sets `kind = QUOT_LITERAL`
  - Doesn't create cells/blob buffers or switch buffers
- ✅ Token capture in compilation loop
  - When `buffer_stack_depth > 0`, tokens are captured instead of compiled
  - `quot_append_token()` helper handles token storage and array growth
- ✅ `quot_compile_with_context()` function
  - Takes QUOT_LITERAL and type stack context
  - Compiles tokens into cells/blob using provided types
  - Upgrades quotation to QUOT_TYPED after compilation
- ✅ Immediate word handlers updated
  - `compile_times_until` compiles both quotations with context
  - `compile_if` compiles both quotations with context
- ✅ Successfully tested with `test/test_times_until_counter.march`
  - Quotation `( 3 < )` captures tokens, compiles at use site
  - Polymorphic `<` resolves correctly with type context from parent word
  - Compilation succeeds without errors!

### What Was NOT Done (No Longer Needed)
Current behavior: `( ... )` compiles tokens immediately into cells/blob
Desired behavior: `( ... )` stores tokens, compiles later at use site with type context


## Plan

What needs to change:

1. **compile_lparen()** (line ~325 in compiler.c)
   - Allocate `quot->tokens` array (start with capacity 16)
   - Set `quot->kind = QUOT_LITERAL`
   - Don't create cell/blob buffers yet
   - Don't switch buffers (keep compiling parent)

2. **Main compilation loop** (compile_definition, line ~1065)
   - Check `if (comp->buffer_stack_depth > 0)` → inside quotation
   - Instead of compiling token, append to `quot->tokens[]`
   - Handle nested `(` and `)` specially

3. **compile_rparen()** (line ~375)
   - For QUOT_LITERAL: just finalize token count, don't compile
   - For QUOT_TYPED: compile with type markers (future)

4. **New function: quot_compile_with_context()**
   ```c
   bool quot_compile_with_context(
       compiler_t* comp,
       quotation_t* quot,
       type_id_t* type_stack,
       int type_stack_depth
   );
   ```
   - Takes saved tokens and type context
   - Compiles tokens into cells/blob using provided type stack
   - Returns compiled quotation with inferred output types

5. **Update immediate word handlers**
   - `compile_times_until()`: call quot_compile_with_context for both quots
   - `compile_if()`: call quot_compile_with_context for both quots
   - Count-based `compile_times()`: already works (pops quot from stack)

### Current Issues

Quotations like `( 3 < )` fail to compile because:
- Type stack is reset when entering quotation (line 365)
- Polymorphic `<` has no concrete types to bind to

With QUOT_LITERAL:
- Tokens saved: `[TOK_NUMBER "3", TOK_WORD "<"]`
- When `times` calls quot_compile_with_context with parent type stack
- Can resolve `<` to `i64 i64 -> bool` based on runtime stack

### Test Cases to Try After Implementation
```march
: test-until
  0                    # Initial value
  ( 5 < )              # Condition quot - uses stack value
  ( 1 + )              # Body quot - modifies stack value
  times
;
```

Should compile to until-style loop that counts 0→5.

### Files to Modify
- `src/compiler.c`: compile_lparen, compile_rparen, compile_definition, new quot_compile_with_context
- `src/compiler.h`: Already updated with type definitions

### Notes
- `comp->quot_stack[]` holds quotations during compilation
- `comp->buffer_stack_depth` tracks nesting (0 = root, >0 = inside quot)
- Token capture needs to handle nested quotations correctly
- May want helper: `quot_append_token(quot_t*, token_t*)`
