It’s not impossible 🙂 — you’ve just bumped into the exact tradeoff line that every “typed, incremental, Forth-ish” system hits:

| **You want per-word, ahead-of-time compilation and whole-program-level information (types, lifetimes, effects).**
| But per-word AOT only ever sees the word, not all its call sites. So some info has to stay symbolic.

The way out is not “give up,” it’s “decide what stays symbolic.” Let me show you a shape that works and still feels like Forth.

I’ll break it into 5 parts:

1. What is actually hard
2. What you can fix per-word
3. What must stay symbolic
4. Two workable designs
5. Why this is enough for your language

## 1. What is actually hard

The hard part is not “statically typed Forth.”
The hard part is:

* polymorphic words (like your ddup)
* resource-sensitive ops (dup/drop that depend on the type’s ownership model)
* region / RC lowering (needs to know if the value escapes)

Those **all want call-site info**.
Per-word AOT doesn’t have it.

So unless you make your language monomorphic-everywhere (not fun), you must leave **placeholders** in the compiled word that get resolved later.

That’s the whole trick.

## 2. What you can do per word (fully AOT)

You can 100% do this ahead of time, for each word:

* parse + typecheck against symbolic type variables
* compute stack effect (with symbols)
* assign frame slots for allocations
* emit IR/bytecode with symbolic RC/region ops
* generate the epilogue (frees for non-escaped slots)
* record requirements: “this word needs T: Dup,” “this word returns a ref-typed value,” “slot 2 escapes,” etc.

So the word is compiled into something like:

```
WORD ddup
  requires: [ T: Dup ]
  stack in:  ( T )
  stack out: ( T T T )
  code:
    RC_retain<T> %0
    push %0
    RC_retain<T> %0
    push %0
  epilogue:
    ; none
```

That’s a real, runnable artifact — just not fully specialized.

## 3. What must stay symbolic

There are three things you can’t nail down inside the word without whole-program info:

* Concrete type for T. Solution: keep T symbolic; caller supplies it
* Exact RC operation (no-op vs retain/release). Solution: lower at call site or via a tiny typeops table
* Exact region / promote behavior. Solution: default to “returns escape to caller’s region”; caller is the one who puts it in its own frame slot

So your per-word AOT artifact is parametric. That’s okay! That’s what generics are.

## 4. Two workable designs

Design A — “Parametric bytecode + call-site adapters”

* Every word is compiled once to parametric bytecode
* Bytecode contains ops like RC_RETAIN tyvar(0) and PROMOTE tyvar(0)
* At every call site, the compiler/emitter knows the actual type (because the caller is being compiled now) and emits a tiny adapter:
	* if tyvar(0) is immediate → adapter erases RC ops
	* if tyvar(0) is ref → adapter leaves them in, maybe inlines
	* if word returns a ref → adapter stashes it in caller’s frame slot
* The callee doesn’t need to be recompiled; adapters do the specializing

This is **per-word AOT**; the only thing not frozen is the adapter, which is per-call.

This is very similar to how some typed concatenative languages and also how interface method tables in Go/OCaml-ish runtimes work.

Design B — “Monomorphize on first use (per word, not whole program)”

* You store the parametric definition
* When you first call ddup<i64>, you generate a concrete version ddup$i64
* Later, first time you call ddup<Box<Node>>, you generate ddup$BoxNode
* You never need the whole program — just the word + the concrete instantiation
* This is exactly what you were trying to avoid with “full AOT,” but note: you’re not recompiling the world, you’re just instantiating that one word.

This is more like “lazy AOT” or “AOT-per-specialization.”

If you’re okay shipping multiple versions of the same word, this is the cleanest mental model.

(NOTE: I like DESIGN B!)

## 5. Why this is enough

Let’s map it to your pain points.

### Pain: “drop has to pick drop-ref vs drop-free”

Per-word AOT output: DROP_GENERIC tyvar(0)
Call-site adapter: “for tyvar(0)=i64 → erase; for tyvar(0)=RC → emit dec+maybe-free”

So drop is written once, compiled once, and specialized locally where it’s called.

### Pain: “ddup needs the concrete type”

Per-word AOT: DUP_GENERIC tyvar(0) twice
Call site: “I know tyvar(0) = FooRef” → emit retain(FooRef) twice
No whole-program pass needed.

### Pain: “region close needs to know what to free”

Per-word AOT: you already did this — you recorded every alloc made in this word.
At the end of the word, you emit frees for all non-escapes.
This is purely local — no whole-program knowledge needed.

So the only thing you couldn’t do 100% locally was: “is this retain actually necessary for this concrete type?”
Answer: push that to the call site or to a later small pass.

## Key mental shift

Per-word AOT can produce parametric code.
Parametric code is still “compiled.”
It just needs a tiny bit of info at the call site to finish the job.

That’s the same trick as:

* C++ templates (instantiated later)
* ML functors (applied later)
* Go interfaces (caller passes method table)
* even ColorForth-ish “late coloring” (color means “how to interpret this later”)

So no, you’re not chasing an impossibility — you’re just trying to get whole-program precision from per-word compilation,
and the compromise is: **keep the ops that depend on the concrete type symbolic, and resolve them where the type is known.**

