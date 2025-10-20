# Types

*This is an evolving document*.

### 1) Plain 0-arg function (baseline)

* Type: `Thunk<A>` or `() -> A`
* Call: `force : Thunk<A> -> A`
* Pros: simplest; fits any ML/Haskell/Rust-ish type checker.
* Cons: doesn’t expose stack effects, staging, or code inspection.

### 2) Stack-effect typed thunks (concatenative-friendly)

Type a thunk by its stack transformer:

* Type: `Thunk[ Σ_in ⟶ Σ_out ]` e.g. `Thunk[ (Int, Str) ⟶ (Int) ]` or more compactly `Thunk[ Int Str — Int ]`.
* Call: `call : Thunk[σ⟶τ] ⊗ σ → τ`=
* Compose: `compose : Thunk[σ⟶τ] × Thunk[τ⟶ρ] → Thunk[σ⟶ρ]`

Map example: If map expects a function `α→β`, in stack form it expects a thunk `Thunk[ α — β ]`.

This matches the “type stack shadows data stack” idea and gives powerful compile-time checks: you can verify before running that a thunk balances the stack.

### 3) Quotation with environment classifiers (typed staging)

When you want macros/metaprogramming (build code, not just run it), use typed quotations:

Syntax: `⟨e⟩` (build code), `~e` (splice), `run e` (stage boundary).

Type: `Code^k[Γ ⊢ τ]` -- “a piece of code at stage `k`, which expects environment `Γ`, and yields `τ`”.

Typing rules (sketch):

* If `Γ ⊢ e : τ` (at stage k), then `· ⊢ ⟨e⟩ : Code^k[Γ ⊢ τ]` (at stage k+1).
* If `Δ ⊢ p : Code^k[Γ ⊢ τ]` and `Δ ⊢ ρ : Γ`, then `Δ ⊢ run(p, ρ) : τ`.
* Splice: inside `⟨ … ~p … ⟩`, require `p : Code^k[Γ ⊢ σ]` and contexts match.
This is the essence of MetaOCaml/F#/Scala-3 quotes, but adapted to your stack world.

### 4) Effects and resources (be honest in the type)

Thunks often do IO, mutate state, allocate, etc. Track that:

Type: `Thunk[A ! E]` or stack style `Thunk[ σ — τ ! E ]`.

Examples:
  `!{IO}` for device I/O, `!{Alloc}` for malloc/free, `!{State<S>}` for a region `S`.

You can use effect rows (`!{IO | e}`) to keep things polymorphic.

### 5) Ownership / lifetimes for captures

*Doubt we will eve need this since we don't have closures.*

If a thunk captures references, make it explicit:

* Borrowed capture: `Thunk[ … ]<'a>` (cannot outlive 'a)
* Owned capture: move into the thunk; no external lifetime needed.
* Linear/affine thunks: ensure one-shot use (great for unique resources, GPU handles, etc.).

### 6) Reflection boundary

Decide if a thunk is:

* Opaque closure (you can only call/compose it), or
* Reifiable code (you can inspect/optimize it).

Often you have two kinds:

* Thunk[…] (opaque, efficient closure)
* Quote[…] / Code[…] (inspectable AST with types).

Provide compile : Code → Thunk and reify : Thunk → Option<Code> if supported.

### 7) Minimal typing rules (stack flavor)

Let stacks be type lists. Judgments look like `Γ ⊢ e : Σ_in ⟶ Σ_out ! E`.

* Quote introduce

```
    Γ ⊢ e : Σ ⟶ Τ ! E
    ───────────────────────────────
    Γ ⊢ [ e ] : Thunk[ Σ ⟶ Τ ! E ]
```

* Call 

    Γ ⊢ t : Thunk[ Σ ⟶ Τ ! E ]    Γ ⊢ s : Σ
    ─────────────────────────────────────────
             Γ ⊢ call(t, s) : Τ ! E
```

* Compose

```
    Γ ⊢ f : Thunk[ Σ ⟶ Τ ! E ]
    Γ ⊢ g : Thunk[ Τ ⟶ Ρ ! F ]
    ────────────────────────────────
    Γ ⊢ f ∘ g : Thunk[ Σ ⟶ Ρ ! (E ∪ F) ]
```

### 8) How this feels in March-ish syntax

Assume [ … ] builds a thunk, and call executes it.

```forth
-- Types (informal)
-- [ ... ] : Thunk[ Σ_in — Σ_out ! E ]

: inc   [ Int — Int ]  ( n -- n+1 )  ... ;
: sq    [ Int — Int ]  ( n -- n*n )  ... ;
: >str  [ Int — Str ]  ... ;

: compose ( Thunk[a—b], Thunk[b—c] — Thunk[a—c] ) ... ;

: map ( Vec α, Thunk[ α — β ] — Vec β )
  -- Applies the stack-effect thunk elementwise
  ... ;

: demo
  [ Int — Str ] \ f
    [ inc ] [ sq ] compose [ >str ] compose  -> f
    [ 3 ] f call   -- push 3, then apply f; stack now has "16"
;
```

with effects:

```
: print [ Str — Unit ! {IO} ] ... ;

: show_square_plus_one
  [ Int — Unit ! {IO} ]
  [ inc ] [ sq ] compose  -- Int — Int
  [ >str ] compose        -- Int — Str
  [ print ] compose       -- Int — Unit ! {IO}
;
```

### 9) Polymorphism, rank-N when useful

Higher-order consumers like withResource often require the function not to escape:

```
withResource : (∀r. Resource r -> Thunk[ σ — τ ! {State<r>} ]) -> Thunk[ σ — τ ]
```

That `∀r` prevents capturing `r` outside the scope (classic region trick).

### 10) Codegen strategy

* Opaque thunks: compile to closures (env pointer + code pointer). Inline when small.
* Quotes: keep as typed AST. Provide:
  * constant folding, dead-code elimination,
  * stack-effect checker (guarantees balance),
  * partial evaluation (splice known thunks),
  * defunctionalization (turn a finite set of thunks into a tagged enum + jump table).
* Bridging: `specialize : Code[Γ ⊢ τ] × Env Γ → Thunk[ [] — τ ]`.

### 11) Hygiene & splicing

If you support `⟨ … ~q … ⟩`, make splices safe:

Names introduced inside a quote are hygienic by default.

Provide explicit escape hatches for intentional capture.


### 12) Practical defaults (what I’d ship first)

1. Start with stack-effect thunks: `Thunk[σ—τ]`, `call`, `compose`, `map`.
2. Add effects as annotations; enforce at compile time.
3. Add typed quotes (`Code[Γ⊢τ]`) for macros and staged optimization, plus a compile that produces an optimized Thunk.
4. If you want borrow/region safety, add lifetimes only for thunks that capture references.


## Example Implimentation

```c
// thunks.c — single-file, drop-in mini-runtime for stack-effect thunks
// Build: cc -std=c11 -O2 thunks.c -o thunks
// Run:   ./thunks
// Remove main() if you’re embedding.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* =========================
   Types, Values, Stack
   ========================= */

typedef enum {
  T_UNIT = 0,
  T_INT,
  T_STR,
  T_VEC,
  T_ANY /* wildcard for debug checks / generics */
} TypeTag;

typedef struct Vec Vec;

typedef struct {
  TypeTag tag;
  union {
    int64_t i;
    struct { char *ptr; size_t len; } s;
    Vec *vec;
  } as;
} Value;

static Value V_unit(void){ Value v; v.tag=T_UNIT; return v; }
static Value V_int(int64_t x){ Value v; v.tag=T_INT; v.as.i=x; return v; }
static Value V_str_take(char* p, size_t n){ Value v; v.tag=T_STR; v.as.s.ptr=p; v.as.s.len=n; return v; }
static Value V_vec(Vec* vp){ Value v; v.tag=T_VEC; v.as.vec=vp; return v; }

/* Simple vector */
struct Vec {
  size_t len, cap;
  Value *data;
};

static Vec* vec_new(size_t cap){
  Vec* v = (Vec*)calloc(1, sizeof(Vec));
  v->cap = cap?cap:4;
  v->data = (Value*)malloc(sizeof(Value)*v->cap);
  v->len = 0;
  return v;
}
static void vec_push(Vec* v, Value x){
  if(v->len==v->cap){
    v->cap = v->cap? v->cap*2 : 4;
    v->data = (Value*)realloc(v->data, sizeof(Value)*v->cap);
  }
  v->data[v->len++] = x;
}

/* Very small GC-ish helpers (demo-grade) */
static void value_free(Value v){
  if(v.tag==T_STR) { free(v.as.s.ptr); }
  else if(v.tag==T_VEC) {
    Vec* vc=v.as.vec;
    for(size_t i=0;i<vc->len;i++) value_free(vc->data[i]);
    free(vc->data);
    free(vc);
  }
}

typedef struct {
  Value *data;
  size_t top, cap;
} Stack;

static void stack_init(Stack* s){ s->top=0; s->cap=16; s->data=(Value*)malloc(sizeof(Value)*s->cap); }
static void stack_free(Stack* s){
  for(size_t i=0;i<s->top;i++) value_free(s->data[i]);
  free(s->data);
}
static void push(Stack* s, Value v){
  if(s->top==s->cap){
    s->cap = s->cap? s->cap*2 : 16;
    s->data=(Value*)realloc(s->data, sizeof(Value)*s->cap);
  }
  s->data[s->top++] = v;
}
static Value pop(Stack* s){
  assert(s->top>0 && "stack underflow");
  return s->data[--s->top];
}
static Value* peek(Stack* s, size_t i_from_top){
  assert(i_from_top < s->top);
  return &s->data[s->top-1 - i_from_top];
}

/* =========================
   Stack-effect TYPE metadata
   ========================= */

typedef struct {
  TypeTag *items;
  size_t len;
} TypeList;

static TypeList typelist_new(size_t n){
  TypeList t; t.len=n;
  t.items = (TypeTag*)malloc(sizeof(TypeTag)*n);
  return t;
}
static TypeList typelist_copy(TypeList a){
  TypeList t = typelist_new(a.len);
  memcpy(t.items, a.items, sizeof(TypeTag)*a.len);
  return t;
}
static void typelist_free(TypeList t){ free(t.items); }

static int type_match(TypeTag need, TypeTag have){
  if(need==T_ANY) return 1;
  return need==have;
}

/* =========================
   Effects (bitset)
   ========================= */

typedef enum {
  E_NONE  = 0,
  E_IO    = 1<<0,
  E_ALLOC = 1<<1,
  E_STATE = 1<<2
} EffectBits;

/* =========================
   Thunks
   =========================
   code(Stack*, env, *eff_accum) returns 0 on ok, nonzero on error.
*/

typedef struct Thunk Thunk;

struct Thunk {
  int   (*code)(Stack*, void* env, uint32_t* eff_accum);
  void  *env;
  TypeList in_sig;
  TypeList out_sig;
  uint32_t effects;   /* declared effect bits */
  const char *name;   /* optional for debugging */
};

/* Debug check: verify the top of stack matches in_sig (from left = bottom) */
static int check_stack_for_call(const Thunk* t, const Stack* s){
#ifndef NDEBUG
  if(s->top < t->in_sig.len) {
    fprintf(stderr, "[typecheck] %s: needs %zu args, have %zu\n",
            t->name?t->name:"<thunk>", t->in_sig.len, s->top);
    return 0;
  }
  for(size_t i=0;i<t->in_sig.len;i++){
    TypeTag need = t->in_sig.items[i];
    TypeTag have = s->data[s->top - t->in_sig.len + i].tag;
    if(!type_match(need, have)){
      fprintf(stderr, "[typecheck] %s: arg %zu: need %d, have %d\n",
              t->name?t->name:"<thunk>", i, (int)need, (int)have);
      return 0;
    }
  }
#else
  (void)t; (void)s;
#endif
  return 1;
}

/* Call a thunk: pops in_sig values (consumed by the code) and expects code to push out_sig */
static int call_thunk(Thunk* t, Stack* s, uint32_t* eff_accum){
  if(!check_stack_for_call(t, s)) return -1;
  int rc = t->code(s, t->env, eff_accum);
#ifndef NDEBUG
  if(rc==0 && s->top < t->out_sig.len){
    fprintf(stderr, "[typecheck] %s: pushed fewer than declared outputs\n", t->name?t->name:"<thunk>");
    return -2;
  }
#endif
  if(rc==0) *eff_accum |= t->effects;
  return rc;
}

/* Helper for constructing thunks */
typedef int (*ThunkFn)(Stack*, void*, uint32_t*);

static Thunk make_thunk(ThunkFn f, void* env, TypeList in, TypeList out, uint32_t eff, const char* name){
  Thunk t;
  t.code=f; t.env=env; t.in_sig=in; t.out_sig=out; t.effects=eff; t.name=name;
  return t;
}

/* =========================
   Composition: g ∘ f
   ========================= */

typedef struct { Thunk f, g; } PairEnv;

static int composed_code(Stack* s, void* env, uint32_t* eff){
  PairEnv* p = (PairEnv*)env;
  int rc = call_thunk(&p->f, s, eff);
  if(rc) return rc;
  rc = call_thunk(&p->g, s, eff);
  return rc;
}

static Thunk compose_thunk(Thunk f, Thunk g, const char* name){
  /* Compose type metadata: in = f.in; out = g.out.
     (We assume f.out == g.in; checked only in debug.) */
#ifndef NDEBUG
  if(f.out_sig.len != g.in_sig.len){
    fprintf(stderr, "[compose] arity mismatch: %s then %s\n",
            f.name?f.name:"<f>", g.name?g.name:"<g>");
  } else {
    for(size_t i=0;i<f.out_sig.len;i++){
      if(!type_match(g.in_sig.items[i], f.out_sig.items[i])){
        fprintf(stderr, "[compose] type mismatch at %zu: %d -> %d\n",
                i, (int)f.out_sig.items[i], (int)g.in_sig.items[i]);
      }
    }
  }
#endif
  PairEnv* penv = (PairEnv*)malloc(sizeof(PairEnv));
  *penv = (PairEnv){ .f=f, .g=g };
  TypeList in  = typelist_copy(f.in_sig);
  TypeList out = typelist_copy(g.out_sig);
  uint32_t eff = f.effects | g.effects;
  return make_thunk(&composed_code, penv, in, out, eff, name);
}

/* =========================
   map_vec :  (Vec α, Thunk[ α — β ]) —> Vec β
   ========================= */

typedef struct { Thunk fn; } MapEnv;
static int map_code(Stack* s, void* env, uint32_t* eff){
  MapEnv* me = (MapEnv*)env;
  Thunk fn = me->fn;

  /* Stack: [ ... , Vec ] -> [ ... , Vec ] */
  if(s->top < 1 || s->data[s->top-1].tag != T_VEC){
    fprintf(stderr, "[map] need a Vec on stack\n");
    return -1;
  }
  Value v = pop(s);
  Vec* in = v.as.vec;

  Vec* out = vec_new(in->len);
  Stack tmp; stack_init(&tmp);

  /* Best-effort type sanity: expect fn.in_sig.len == 1 and fn.out_sig.len == 1 */
#ifndef NDEBUG
  if(fn.in_sig.len!=1 || fn.out_sig.len!=1){
    fprintf(stderr, "[map] fn must be Thunk[ a — b ]\n");
    stack_free(&tmp);
    value_free(v);
    return -2;
  }
#endif

  for(size_t i=0;i<in->len;i++){
    /* push element, call fn, pop result */
    push(&tmp, in->data[i]);
    int rc = call_thunk(&fn, &tmp, eff);
    if(rc){ stack_free(&tmp); value_free(V_vec(in)); value_free(V_vec(out)); return rc; }
    Value r = pop(&tmp);
    vec_push(out, r); /* take ownership of r */
  }

  stack_free(&tmp);
  /* free input vec container (not its moved values) */
  free(in->data); free(in);

  push(s, V_vec(out));
  *eff |= fn.effects;
  return 0;
}

static Thunk make_map(Thunk fn){
  MapEnv* env = (MapEnv*)malloc(sizeof(MapEnv));
  env->fn = fn;
  TypeList in  = typelist_new(1);  in.items[0]=T_VEC;
  TypeList out = typelist_new(1); out.items[0]=T_VEC;
  return make_thunk(&map_code, env, in, out, fn.effects, "map");
}

/* =========================
   Utilities
   ========================= */

static char* dup_cstr(const char* s){
  size_t n = strlen(s);
  char* p = (char*)malloc(n+1);
  memcpy(p, s, n+1);
  return p;
}
static char* int_to_str_alloc(int64_t x, size_t* outlen){
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "%lld", (long long)x);
  char* p = (char*)malloc((size_t)n+1);
  memcpy(p, buf, (size_t)n+1);
  if(outlen) *outlen = (size_t)n;
  return p;
}

/* =========================
   Demo library thunks
   ========================= */

/* inc : [ Int — Int ] */
static int inc_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s);
  assert(v.tag==T_INT);
  int64_t r = v.as.i + 1;
  push(s, V_int(r));
  return 0;
}
static Thunk make_inc(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&inc_code, NULL, in, out, E_NONE, "inc");
}

/* sq : [ Int — Int ] */
static int sq_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_INT);
  int64_t r = v.as.i * v.as.i;
  push(s, V_int(r));
  return 0;
}
static Thunk make_sq(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&sq_code, NULL, in, out, E_NONE, "sq");
}

/* >str : [ Int — Str ] */
static int tostr_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_INT);
  size_t n=0; char* p = int_to_str_alloc(v.as.i, &n);
  push(s, V_str_take(p,n));
  return 0;
}
static Thunk make_to_str(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_STR;
  return make_thunk(&tostr_code, NULL, in, out, E_ALLOC, ">str");
}

/* print : [ Str — Unit ! {IO} ] */
static int print_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_STR);
  fwrite(v.as.s.ptr, 1, v.as.s.len, stdout);
  fputc('\n', stdout);
  /* v’s string is consumed; free it */
  free(v.as.s.ptr);
  push(s, V_unit());
  return 0;
}
static Thunk make_print(void){
  TypeList in=typelist_new(1); in.items[0]=T_STR;
  TypeList out=typelist_new(1); out.items[0]=T_UNIT;
  return make_thunk(&print_code, NULL, in, out, E_IO, "print");
}

/* lift_const_int : [ — Int ] */
typedef struct { int64_t k; } KEnv;
static int konst_int_code(Stack* s, void* env, uint32_t* eff){
  (void)eff;
  KEnv* k=(KEnv*)env;
  push(s, V_int(k->k));
  return 0;
}
static Thunk make_const_int(int64_t k){
  KEnv* e=(KEnv*)malloc(sizeof(KEnv));
  e->k=k;
  TypeList in=typelist_new(0);
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&konst_int_code, e, in, out, E_NONE, "const-int");
}

/* =========================
   Example program
   ========================= */

#ifndef THUNKS_NO_MAIN
int main(void){
  /* Build pipeline: show_square_plus_one = print ∘ >str ∘ sq ∘ inc
     Then call with const 3.
   */
  Thunk inc = make_inc();
  Thunk sq  = make_sq();
  Thunk ts  = make_to_str();
  Thunk pr  = make_print();

  Thunk f = compose_thunk(inc, sq,  "sq∘inc");
  Thunk g = compose_thunk(f,  ts,   ">str∘sq∘inc");
  Thunk h = compose_thunk(g,  pr,   "print∘>str∘sq∘inc");

  Stack S; stack_init(&S);
  uint32_t eff=0;

  /* Push the argument (or use a const-thunk) */
  push(&S, V_int(3));

  /* Call */
  int rc = call_thunk(&h, &S, &eff);
  if(rc){ fprintf(stderr, "call failed: %d\n", rc); }

  /* Expect Unit on stack */
  Value u = pop(&S); (void)u;

  /* Demo: map over a vector */
  Vec* v = vec_new(0);
  vec_push(v, V_int(1));
  vec_push(v, V_int(2));
  vec_push(v, V_int(5));
  push(&S, V_vec(v));

  Thunk to_str = make_to_str();
  Thunk map_str = make_map(to_str);              /* Vec<Int> — Vec<Str> */
  Thunk printer = make_print();                  /* Str — Unit */
  /* print_each = call print per element: we’ll map (>str), then manually print */
  rc = call_thunk(&map_str, &S, &eff);           /* [Vec<Int>] -> [Vec<Str>] */
  if(rc){ fprintf(stderr, "map failed: %d\n", rc); goto done; }

  /* Expand Vec<Str> and print each (simple for-loop here) */
  Value vv = pop(&S); assert(vv.tag==T_VEC);
  Vec* vs = vv.as.vec;
  for(size_t i=0;i<vs->len;i++){
    push(&S, vs->data[i]);          /* Str */
    rc = call_thunk(&printer, &S, &eff);  /* prints one string */
    if(rc){ fprintf(stderr, "print failed: %d\n", rc); }
    (void)pop(&S); /* Unit */
  }
  /* clean up leftover vector shell */
  free(vs->data); free(vs);

done:
  stack_free(&S);

  /* Free composed environments & typelists (demo-grade; in a real lib you’d
     provide destroy_thunk to recursively free env + type lists). */
  typelist_free(inc.in_sig); typelist_free(inc.out_sig);
  typelist_free(sq.in_sig);  typelist_free(sq.out_sig);
  typelist_free(ts.in_sig);  typelist_free(ts.out_sig);
  typelist_free(pr.in_sig);  typelist_free(pr.out_sig);

  typelist_free(f.in_sig); typelist_free(f.out_sig); free(((PairEnv*)f.env));
  typelist_free(g.in_sig); typelist_free(g.out_sig); free(((PairEnv*)g.env));
  typelist_free(h.in_sig); typelist_free(h.out_sig); free(((PairEnv*)h.env));

  printf("\nEffects observed: 0x%02X (E_IO=%d, E_ALLOC=%d)\n",
         eff, (int)!!(eff&E_IO), (int)!!(eff&E_ALLOC));
  return 0;
}
#endif
```

What you get (quick tour)

Stack-effect thunks: each Thunk carries in_sig and out_sig (lists of TypeTags). In debug builds, call_thunk checks the top-of-stack against the thunk’s input signature before executing.

Composition: compose_thunk(f, g) builds a new thunk whose code runs f then g. It merges effects.

Effects: just a bitmask you can grow (E_IO, E_ALLOC, E_STATE, …). The caller accumulates them in eff_accum.

map: make_map(fn) returns a thunk Vec — Vec that applies a 1-in/1-out fn elementwise (using an internal scratch stack).

Value system: integers, strings (owned), vectors, and unit. A tiny value_free clears memory; demo code is careful to transfer ownership (e.g., print consumes the string).

Examples included: inc, sq, >str, print, and the pipeline print∘>str∘sq∘inc. There’s also a small map demo over [1,2,5].

How to plug in

If your runtime already has Stack, Value, and a type/tag system:

Replace the Value/TypeTag bits with yours.

Keep Thunk as-is (or rename) and wire call_thunk, compose_thunk, and make_map.

Swap the demo thunks (inc, sq, >str, print) for your primitives.

If your language already uses [ … ] to build thunks, this C layer is your closure ABI: your compiler can generate small C shims with ThunkFn signatures and fill in in_sig/out_sig from your type checker.

If you want, send me a slice of your current C (or Zig/Rust + C FFI) runtime — I can tailor this to your exact Value, GC/arena, and error model, and add a proper destroy_thunk() that frees envs and type lists recursively.

-----

## Advance Example Implmentation

More advanced ideas that we might never need -- closure and borrowing.

*DON'T NEED TO READ PAST THIS POINT (YET).*

```
// thunks.c — single-file, drop-in mini-runtime for stack-effect thunks
// Build: cc -std=c11 -O2 thunks.c -o thunks
// Run:   ./thunks
// Remove main() if you’re embedding.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* =========================
   Types, Values, Stack
   ========================= */

typedef enum {
  T_UNIT = 0,
  T_INT,
  T_STR,
  T_VEC,
  T_ANY /* wildcard for debug checks / generics */
} TypeTag;

typedef struct Vec Vec;

typedef struct {
  TypeTag tag;
  union {
    int64_t i;
    struct { char *ptr; size_t len; } s;
    Vec *vec;
  } as;
} Value;

static Value V_unit(void){ Value v; v.tag=T_UNIT; return v; }
static Value V_int(int64_t x){ Value v; v.tag=T_INT; v.as.i=x; return v; }
static Value V_str_take(char* p, size_t n){ Value v; v.tag=T_STR; v.as.s.ptr=p; v.as.s.len=n; return v; }
static Value V_vec(Vec* vp){ Value v; v.tag=T_VEC; v.as.vec=vp; return v; }

/* Simple vector */
struct Vec {
  size_t len, cap;
  Value *data;
};

static Vec* vec_new(size_t cap){
  Vec* v = (Vec*)calloc(1, sizeof(Vec));
  v->cap = cap?cap:4;
  v->data = (Value*)malloc(sizeof(Value)*v->cap);
  v->len = 0;
  return v;
}
static void vec_push(Vec* v, Value x){
  if(v->len==v->cap){
    v->cap = v->cap? v->cap*2 : 4;
    v->data = (Value*)realloc(v->data, sizeof(Value)*v->cap);
  }
  v->data[v->len++] = x;
}

/* Very small GC-ish helpers (demo-grade) */
static void value_free(Value v){
  if(v.tag==T_STR) { free(v.as.s.ptr); }
  else if(v.tag==T_VEC) {
    Vec* vc=v.as.vec;
    for(size_t i=0;i<vc->len;i++) value_free(vc->data[i]);
    free(vc->data);
    free(vc);
  }
}

typedef struct {
  Value *data;
  size_t top, cap;
} Stack;

static void stack_init(Stack* s){ s->top=0; s->cap=16; s->data=(Value*)malloc(sizeof(Value)*s->cap); }
static void stack_free(Stack* s){
  for(size_t i=0;i<s->top;i++) value_free(s->data[i]);
  free(s->data);
}
static void push(Stack* s, Value v){
  if(s->top==s->cap){
    s->cap = s->cap? s->cap*2 : 16;
    s->data=(Value*)realloc(s->data, sizeof(Value)*s->cap);
  }
  s->data[s->top++] = v;
}
static Value pop(Stack* s){
  assert(s->top>0 && "stack underflow");
  return s->data[--s->top];
}
static Value* peek(Stack* s, size_t i_from_top){
  assert(i_from_top < s->top);
  return &s->data[s->top-1 - i_from_top];
}

/* =========================
   Stack-effect TYPE metadata
   ========================= */

typedef struct {
  TypeTag *items;
  size_t len;
} TypeList;

static TypeList typelist_new(size_t n){
  TypeList t; t.len=n;
  t.items = (TypeTag*)malloc(sizeof(TypeTag)*n);
  return t;
}
static TypeList typelist_copy(TypeList a){
  TypeList t = typelist_new(a.len);
  memcpy(t.items, a.items, sizeof(TypeTag)*a.len);
  return t;
}
static void typelist_free(TypeList t){ free(t.items); }

static int type_match(TypeTag need, TypeTag have){
  if(need==T_ANY) return 1;
  return need==have;
}

/* =========================
   Effects (bitset)
   ========================= */

typedef enum {
  E_NONE  = 0,
  E_IO    = 1<<0,
  E_ALLOC = 1<<1,
  E_STATE = 1<<2
} EffectBits;

/* =========================
   Thunks
   =========================
   code(Stack*, env, *eff_accum) returns 0 on ok, nonzero on error.
*/

typedef struct Thunk Thunk;

struct Thunk {
  int   (*code)(Stack*, void* env, uint32_t* eff_accum);
  void  *env;
  TypeList in_sig;
  TypeList out_sig;
  uint32_t effects;   /* declared effect bits */
  const char *name;   /* optional for debugging */
};

/* Debug check: verify the top of stack matches in_sig (from left = bottom) */
static int check_stack_for_call(const Thunk* t, const Stack* s){
#ifndef NDEBUG
  if(s->top < t->in_sig.len) {
    fprintf(stderr, "[typecheck] %s: needs %zu args, have %zu\n",
            t->name?t->name:"<thunk>", t->in_sig.len, s->top);
    return 0;
  }
  for(size_t i=0;i<t->in_sig.len;i++){
    TypeTag need = t->in_sig.items[i];
    TypeTag have = s->data[s->top - t->in_sig.len + i].tag;
    if(!type_match(need, have)){
      fprintf(stderr, "[typecheck] %s: arg %zu: need %d, have %d\n",
              t->name?t->name:"<thunk>", i, (int)need, (int)have);
      return 0;
    }
  }
#else
  (void)t; (void)s;
#endif
  return 1;
}

/* Call a thunk: pops in_sig values (consumed by the code) and expects code to push out_sig */
static int call_thunk(Thunk* t, Stack* s, uint32_t* eff_accum){
  if(!check_stack_for_call(t, s)) return -1;
  int rc = t->code(s, t->env, eff_accum);
#ifndef NDEBUG
  if(rc==0 && s->top < t->out_sig.len){
    fprintf(stderr, "[typecheck] %s: pushed fewer than declared outputs\n", t->name?t->name:"<thunk>");
    return -2;
  }
#endif
  if(rc==0) *eff_accum |= t->effects;
  return rc;
}

/* Helper for constructing thunks */
typedef int (*ThunkFn)(Stack*, void*, uint32_t*);

static Thunk make_thunk(ThunkFn f, void* env, TypeList in, TypeList out, uint32_t eff, const char* name){
  Thunk t;
  t.code=f; t.env=env; t.in_sig=in; t.out_sig=out; t.effects=eff; t.name=name;
  return t;
}

/* =========================
   Composition: g ∘ f
   ========================= */

typedef struct { Thunk f, g; } PairEnv;

static int composed_code(Stack* s, void* env, uint32_t* eff){
  PairEnv* p = (PairEnv*)env;
  int rc = call_thunk(&p->f, s, eff);
  if(rc) return rc;
  rc = call_thunk(&p->g, s, eff);
  return rc;
}

static Thunk compose_thunk(Thunk f, Thunk g, const char* name){
  /* Compose type metadata: in = f.in; out = g.out.
     (We assume f.out == g.in; checked only in debug.) */
#ifndef NDEBUG
  if(f.out_sig.len != g.in_sig.len){
    fprintf(stderr, "[compose] arity mismatch: %s then %s\n",
            f.name?f.name:"<f>", g.name?g.name:"<g>");
  } else {
    for(size_t i=0;i<f.out_sig.len;i++){
      if(!type_match(g.in_sig.items[i], f.out_sig.items[i])){
        fprintf(stderr, "[compose] type mismatch at %zu: %d -> %d\n",
                i, (int)f.out_sig.items[i], (int)g.in_sig.items[i]);
      }
    }
  }
#endif
  PairEnv* penv = (PairEnv*)malloc(sizeof(PairEnv));
  *penv = (PairEnv){ .f=f, .g=g };
  TypeList in  = typelist_copy(f.in_sig);
  TypeList out = typelist_copy(g.out_sig);
  uint32_t eff = f.effects | g.effects;
  return make_thunk(&composed_code, penv, in, out, eff, name);
}

/* =========================
   map_vec :  (Vec α, Thunk[ α — β ]) —> Vec β
   ========================= */

typedef struct { Thunk fn; } MapEnv;
static int map_code(Stack* s, void* env, uint32_t* eff){
  MapEnv* me = (MapEnv*)env;
  Thunk fn = me->fn;

  /* Stack: [ ... , Vec ] -> [ ... , Vec ] */
  if(s->top < 1 || s->data[s->top-1].tag != T_VEC){
    fprintf(stderr, "[map] need a Vec on stack\n");
    return -1;
  }
  Value v = pop(s);
  Vec* in = v.as.vec;

  Vec* out = vec_new(in->len);
  Stack tmp; stack_init(&tmp);

  /* Best-effort type sanity: expect fn.in_sig.len == 1 and fn.out_sig.len == 1 */
#ifndef NDEBUG
  if(fn.in_sig.len!=1 || fn.out_sig.len!=1){
    fprintf(stderr, "[map] fn must be Thunk[ a — b ]\n");
    stack_free(&tmp);
    value_free(v);
    return -2;
  }
#endif

  for(size_t i=0;i<in->len;i++){
    /* push element, call fn, pop result */
    push(&tmp, in->data[i]);
    int rc = call_thunk(&fn, &tmp, eff);
    if(rc){ stack_free(&tmp); value_free(V_vec(in)); value_free(V_vec(out)); return rc; }
    Value r = pop(&tmp);
    vec_push(out, r); /* take ownership of r */
  }

  stack_free(&tmp);
  /* free input vec container (not its moved values) */
  free(in->data); free(in);

  push(s, V_vec(out));
  *eff |= fn.effects;
  return 0;
}

static Thunk make_map(Thunk fn){
  MapEnv* env = (MapEnv*)malloc(sizeof(MapEnv));
  env->fn = fn;
  TypeList in  = typelist_new(1);  in.items[0]=T_VEC;
  TypeList out = typelist_new(1); out.items[0]=T_VEC;
  return make_thunk(&map_code, env, in, out, fn.effects, "map");
}

/* =========================
   Utilities
   ========================= */

static char* dup_cstr(const char* s){
  size_t n = strlen(s);
  char* p = (char*)malloc(n+1);
  memcpy(p, s, n+1);
  return p;
}
static char* int_to_str_alloc(int64_t x, size_t* outlen){
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "%lld", (long long)x);
  char* p = (char*)malloc((size_t)n+1);
  memcpy(p, buf, (size_t)n+1);
  if(outlen) *outlen = (size_t)n;
  return p;
}

/* =========================
   Demo library thunks
   ========================= */

/* inc : [ Int — Int ] */
static int inc_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s);
  assert(v.tag==T_INT);
  int64_t r = v.as.i + 1;
  push(s, V_int(r));
  return 0;
}
static Thunk make_inc(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&inc_code, NULL, in, out, E_NONE, "inc");
}

/* sq : [ Int — Int ] */
static int sq_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_INT);
  int64_t r = v.as.i * v.as.i;
  push(s, V_int(r));
  return 0;
}
static Thunk make_sq(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&sq_code, NULL, in, out, E_NONE, "sq");
}

/* >str : [ Int — Str ] */
static int tostr_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_INT);
  size_t n=0; char* p = int_to_str_alloc(v.as.i, &n);
  push(s, V_str_take(p,n));
  return 0;
}
static Thunk make_to_str(void){
  TypeList in=typelist_new(1); in.items[0]=T_INT;
  TypeList out=typelist_new(1); out.items[0]=T_STR;
  return make_thunk(&tostr_code, NULL, in, out, E_ALLOC, ">str");
}

/* print : [ Str — Unit ! {IO} ] */
static int print_code(Stack* s, void* env, uint32_t* eff){
  (void)env; (void)eff;
  Value v = pop(s); assert(v.tag==T_STR);
  fwrite(v.as.s.ptr, 1, v.as.s.len, stdout);
  fputc('\n', stdout);
  /* v’s string is consumed; free it */
  free(v.as.s.ptr);
  push(s, V_unit());
  return 0;
}
static Thunk make_print(void){
  TypeList in=typelist_new(1); in.items[0]=T_STR;
  TypeList out=typelist_new(1); out.items[0]=T_UNIT;
  return make_thunk(&print_code, NULL, in, out, E_IO, "print");
}

/* lift_const_int : [ — Int ] */
typedef struct { int64_t k; } KEnv;
static int konst_int_code(Stack* s, void* env, uint32_t* eff){
  (void)eff;
  KEnv* k=(KEnv*)env;
  push(s, V_int(k->k));
  return 0;
}
static Thunk make_const_int(int64_t k){
  KEnv* e=(KEnv*)malloc(sizeof(KEnv));
  e->k=k;
  TypeList in=typelist_new(0);
  TypeList out=typelist_new(1); out.items[0]=T_INT;
  return make_thunk(&konst_int_code, e, in, out, E_NONE, "const-int");
}

/* =========================
   Example program
   ========================= */

#ifndef THUNKS_NO_MAIN
int main(void){
  /* Build pipeline: show_square_plus_one = print ∘ >str ∘ sq ∘ inc
     Then call with const 3.
   */
  Thunk inc = make_inc();
  Thunk sq  = make_sq();
  Thunk ts  = make_to_str();
  Thunk pr  = make_print();

  Thunk f = compose_thunk(inc, sq,  "sq∘inc");
  Thunk g = compose_thunk(f,  ts,   ">str∘sq∘inc");
  Thunk h = compose_thunk(g,  pr,   "print∘>str∘sq∘inc");

  Stack S; stack_init(&S);
  uint32_t eff=0;

  /* Push the argument (or use a const-thunk) */
  push(&S, V_int(3));

  /* Call */
  int rc = call_thunk(&h, &S, &eff);
  if(rc){ fprintf(stderr, "call failed: %d\n", rc); }

  /* Expect Unit on stack */
  Value u = pop(&S); (void)u;

  /* Demo: map over a vector */
  Vec* v = vec_new(0);
  vec_push(v, V_int(1));
  vec_push(v, V_int(2));
  vec_push(v, V_int(5));
  push(&S, V_vec(v));

  Thunk to_str = make_to_str();
  Thunk map_str = make_map(to_str);              /* Vec<Int> — Vec<Str> */
  Thunk printer = make_print();                  /* Str — Unit */
  /* print_each = call print per element: we’ll map (>str), then manually print */
  rc = call_thunk(&map_str, &S, &eff);           /* [Vec<Int>] -> [Vec<Str>] */
  if(rc){ fprintf(stderr, "map failed: %d\n", rc); goto done; }

  /* Expand Vec<Str> and print each (simple for-loop here) */
  Value vv = pop(&S); assert(vv.tag==T_VEC);
  Vec* vs = vv.as.vec;
  for(size_t i=0;i<vs->len;i++){
    push(&S, vs->data[i]);          /* Str */
    rc = call_thunk(&printer, &S, &eff);  /* prints one string */
    if(rc){ fprintf(stderr, "print failed: %d\n", rc); }
    (void)pop(&S); /* Unit */
  }
  /* clean up leftover vector shell */
  free(vs->data); free(vs);

done:
  stack_free(&S);

  /* Free composed environments & typelists (demo-grade; in a real lib you’d
     provide destroy_thunk to recursively free env + type lists). */
  typelist_free(inc.in_sig); typelist_free(inc.out_sig);
  typelist_free(sq.in_sig);  typelist_free(sq.out_sig);
  typelist_free(ts.in_sig);  typelist_free(ts.out_sig);
  typelist_free(pr.in_sig);  typelist_free(pr.out_sig);

  typelist_free(f.in_sig); typelist_free(f.out_sig); free(((PairEnv*)f.env));
  typelist_free(g.in_sig); typelist_free(g.out_sig); free(((PairEnv*)g.env));
  typelist_free(h.in_sig); typelist_free(h.out_sig); free(((PairEnv*)h.env));

  printf("\nEffects observed: 0x%02X (E_IO=%d, E_ALLOC=%d)\n",
         eff, (int)!!(eff&E_IO), (int)!!(eff&E_ALLOC));
  return 0;
}
#endif
```

What you get (quick tour)

Stack-effect thunks: each Thunk carries in_sig and out_sig (lists of TypeTags). In debug builds, call_thunk checks the top-of-stack against the thunk’s input signature before executing.

Composition: compose_thunk(f, g) builds a new thunk whose code runs f then g. It merges effects.

Effects: just a bitmask you can grow (E_IO, E_ALLOC, E_STATE, …). The caller accumulates them in eff_accum.

map: make_map(fn) returns a thunk Vec — Vec that applies a 1-in/1-out fn elementwise (using an internal scratch stack).

Value system: integers, strings (owned), vectors, and unit. A tiny value_free clears memory; demo code is careful to transfer ownership (e.g., print consumes the string).

Examples included: inc, sq, >str, print, and the pipeline print∘>str∘sq∘inc. There’s also a small map demo over [1,2,5].

How to plug in

If your runtime already has Stack, Value, and a type/tag system:

Replace the Value/TypeTag bits with yours.

Keep Thunk as-is (or rename) and wire call_thunk, compose_thunk, and make_map.

Swap the demo thunks (inc, sq, >str, print) for your primitives.

If your language already uses [ … ] to build thunks, this C layer is your closure ABI: your compiler can generate small C shims with ThunkFn signatures and fill in in_sig/out_sig from your type checker.

If you want, send me a slice of your current C (or Zig/Rust + C FFI) runtime — I can tailor this to your exact Value, GC/arena, and error model, and add a proper destroy_thunk() that frees envs and type lists recursively.


