# Syntax Design

From one point of view, March's syntax is a serialization format, since the database is the 
true primary source. This is important to keep in mind -- the format must also be parsed back
into the database.

Common serialization formats, such as YAML or JSON, would work, but are not necessarily the nicest way to read or edit a program. 

It is also technically possible for anyone to create there own serialization format, as long as it stays
true to the basics of the language. 

Of the possibilities, S-Expression stands out a a good match. March programs can be essentially outputed as a Lisp-variant program,
and read in just as easily. To any Lisper the structure of code will be reasonably familair, readable and editable.

And yet, FORTH is the primary inspriation for March. And if we extend the concepts that FORTH originated
it proves a very flexible syntax.

We cover this native March's syntax first, and then come back to a Lisp representation at the end.
In short, I am thinking March can support both text formats "out-of-the-box" -- and others would not be hard to write.

## Core Philosophy

This language is a statically-typed FORTH derivative with context-oriented programming as a central design principle. The syntax must support user-defined parsing words for domain-specific languages while maintaining the ability to perfectly round-trip code through the database.

## The Problem

Traditional compilers are lossy: they parse text → build an AST → compile to a final form, discarding syntactic information in the process. This makes round-tripping (storing code, retrieving it, and displaying it in recognizable form) difficult.

## The Solution: Multi-Context Execution

The language uses **one program with multiple execution contexts**:

### 1. Runtime Context
- Normal FORTH execution
- Push values to stack, execute quotations
- Produces results

### 2. Compilation Context
- Instead of executing to the stack, **emit database operations**
- Parse text → emit INSERT statements for the semantic structure
- Track the **full operation sequence** (which parsing words matched, what they captured, how operations linked)
- Store this reference chain in the database

### 3. Format Context
- Walk the stored operations from the database
- Regenerate text (with optional user-preferred formatting)
- Preserve the original syntactic structure (minus whitespace/formatting)

## Key Insight

By storing the **intermediate operation sequence** (not just the final semantic result), no information is lost. The AST is implicit in the reference chains stored in the database.

## How Parsing Words Fit In

Parsing words (like `:`, `a`, `talks-with`) work uniformly across all contexts:

```forth
a dog talks with "bark" ;
```

In **compile context**, this:
1. Generates operations for the symbol `dog`
2. Generates operations for the string `"bark"`
3. The parsing word `talks-with` generates operations that link them
4. All reference IDs and relationships are stored

In **format context**, walking those stored operations can regenerate:
- The original: `a dog talks with "bark" ;`
- Canonical form: `"bark" dog talks-with a` (or similar)
- Alternative user-preferred formats

## Guards and Type Signatures

Both are context-oriented:

- **Type Signatures** are compile-time constraints, attached to word definitions
- **Guards** are runtime checks, defining execution contexts

Syntax (Conceputal. Not finalized!):

```forth
: myprogram {
  : version "0.1.0" ;     -- constant

  : name = "John" ;       -- variable
  : name ( @name ) ;      -- reader

  $ [ n -- n ]            -- type signature

  @ positive-check        -- guard with thunk/quotation
  : operate ( 2 * ) .     -- word definition via thunk

  @ negative-check
  : operate ( -1 * ) ; 
} ;
```

Multiple words can share the same name under different guards/types, similar to function overloading.

Guards and types are inherited by subsequent definitions until explicitly reset.

## Database Schema (Conceptual)

```
definitions:
  id, name, guards, types, compiled_body

parse_operations:
  id, definition_id, sequence, operation_type, data
```

The `parse_operations` table stores the full trace of parsing decisions, enabling perfect round-tripping.

## Design Goals

1. **Lossless serialization**: Full syntactic information is preserved (minus formatting)
2. **User-defined DSLs**: Parsing words are powerful and composable
3. **Predictable round-tripping**: Code in → database → code out (recognizable)
4. **Uniform evaluation**: One program, multiple contexts (runtime, compile, format)
5. **Static compilation**: Guards are runtime checks; types are compile-time constraints
6. **Context-oriented programming**: Behavior defined by active guards/types, not hard-coded control flow

## Future Considerations

- Whitespace and formatting preservation (optional, via metadata)
- User-defined format preferences/aliases
- Optimization passes on stored operations
- Dependent types (future, more complex)


--- *STOP READING HERE* ----

MORE EXPLORITORY/SPECULATIVE FROM HERE DOWN.

## Native Syntax 

FORTH has a concept of *immediate words*. These words run at compile time -- words like `:` and `FOR`.
Immediate words are cabable of reading from the *token stream*, and thus capable of parsing the stream.
March takes advantage of this, and takes it to a new level, allowing the programmer to create what
are essentially *grammers*.

The *parsing words* use some special built-in words to read in the token stream. These range from 
just getting a fixed number of word tokens from the input buffer, to reading up=to a sentinal word
such as `END`, `;` or even a line break.

Under the hood, all a parsing word efficteivly does is take the tokens it reads from the input buffer
and reorganizes an augments them into an evaluatable stack. Presently the evaluation can occur peicemeal,
so all the parsing does not have to be done at once. Another option is to only allow parsing words to adjust
then toke stream, but not actually execute anything else. Normal processing takes care of it from there.

The most common immediate word is `: ... ;`, which in March has an Logo-insired alternative `to ... end`.

```
-- Traditional FORTH
: hello 
  name "hello" .cr .cr 
;

-- Logo inspired version
to hello 
  name "hello" .cr .cr 
end
```

But we have a conundrum here. What about quotation syntax. Should that be here so it is clearer:

to hello [ name "hello" .cr .cr ] end

These parsing words can get very clever, allowing for all sorts of strange syntax constructions.
For greater readability however, it is advisable to use the pre-exiting syntax as much as possible.
Indeed one could conseivaby program something like:

```
on dog ;
to speaks!
  "bark!"
end

a dog speaks!  --> "bark!"
```

### Considerations

For common words, having short words is beneficial as long as not too many are used.

Words like: 

```
to is a of the on in for
```

These are you pronouns and prepositions, and with a little finegelling, that could be useful
as our primatives for constructing the program. 


### Playground

This is an area for toying with syntax considerations:

```
program is as
  the name is the pretend-library .
  the version is "1.0.0" .

  uses < io math checker > .

  the imports are as
    default is [: hello goodbye :] .
  end

  the song is "Do Re Me Fa so La Te Do" .  (* constant *)
  a name is "Dude" .                       (* variable *)

  on [ checker.checksout? ] .
  it < string > for
    to hello 
      name "hello" .cr .cr 
    end
  end
end

on any .             -- clear context
it any .             -- clear signature

to now [ ... ] !  -- immediate word
```

Notice that word definitions inherit the context and type signature that comes before them.
There is a question as to whether these should be nested.


## Syntax Option 2

This sytax is more "programmery". Everything is a name and then information that specifices how to define that name. 

A few special names (because they don't actully need names) are `?` and `!` (subject to change if need by),
which define current context and current type signature.

Tentively arrays are `[ ]` and thunks are `( )`. Words are thunks with a name. They go into the dictionary.
To define a word that returns a thunk use `(( ))` double thunk.

```
program : the pretend-library
  version : "1.0.0" ;   -- constant

  imports : { io math chk=checker johnslib.simple } ;
  exports : { default=hello } ;

  name = "Dude" ;       -- variable/state

  context = [ checker.checksout? ] ;
  typesig = < string -- > ;

  hello = [ name "hello" . . ] ;

  on [] <> do
    now = ! [ ... ]   -- immediate word
  end

end
```

##

```forth
RETURN-STACK: XT

 ( do-this ) loop 

```



## S-Expressions

NOT WORKED ON YET.

Should we start with an S-Expression format, just so we have something to work with while we finalize the Native Syntax?

```lisp
(program mylib (
  (import console)
  (export hello)

  (when happy?)
  (type text)

  to hello (do "hello" cr)

  (to now ! (do ...))  ;; immediate word
))
