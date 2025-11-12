# Array Design

## Naming

I am not yet sure what the general name for these should be: *array*, *list*,
*vector*, or *sequence*? 

I say *general name* becuase to a March programmer they can often go undistiguished. 
The programmer typically will not need to worry about the underlying implementation;
the compiler will pick the best implmentation to use base on usage analysis. 

## Supported Concrete Types

At the very least, two concrete types are needed: the typical mutable type
and an immutable/persistent type. 

The standard array type is per ususal a fixed size chunk of memory 
(along with a header).

The immutable/persistent type would mostly likely be some variant of a balanced
tree (like HAMT for maps), though maybe a rope structure would work instead?

Multiple types can be supported to fill vaurous efficency nieches,  such a linked-list, 
doubley linked-list, array rope, etc. 

## Compiler Optimization

The compiler will need to track usage, possibly using effect tokens.
The compiler then can decided what type of "array" structure would work
best. Ideally this can be done at compile time, but if necessary can 
occur at JIT time.

(I am referring to the incremental word-by-word compiler here, not
an AOT compiler, which would obviously be able to do the entrie analysis.)

## User Override

The programmer can of course override the compiler and force a concrete type.

## Syntax

Arrays are written using square brackets, e.g. `[ 1 2 3 ]`.

They are natural comprehensions, e.g. `[ 1 2 + ]` results in `[ 3 ]`.

Values on the stack can be pulled into the construction of an array using `_`, 
e.g. `3 [ _ ]` results in [ 3 ]`, and `5 [ _ 1 + ]` results in `[ 6 ]`.

## Note On Old Idea

Originally I had though that comphenesions would pull input from the stack
before the `[` marker and deposit results after it on the true stack top.
For example, `1 1 [ + ]` results in `[ 2 ]`. But I think this is too confusing
and has too strange edge cases. Being explixcit with `_` seems a much better
choice.

