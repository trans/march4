# SYNTAX

The syntax is techincially a serialization format dumped from the database!
Important to keep in mind. The format must also be read back into the database.

So it is possible for anyone to create there own syntax as long as it stays true to the basics of the language.

Or, these formats could be common serialization formats like YAML or JSON.

FORTH is primary inspriation (and design), so the most natural syntax follows it's lead.

But comments are like Haskell.


## Syntax Option 1

*Parsing words* can be define that parse the section of text between them and a `;` or a newline.

It could be writtein in "long form":

```
PROGRAM. pretend-library

IMPORT: io math checker ;

EXPORT. default <- hello ;

STATE. name "Dude" ;

CONTEXT: checker.checksout? ;
  SIGNATURE: string -> ;
	  DEFINE. hello  name "hello" . . ;

CONTEXT. ;     -- clear context
  SIGNATURE. ;   -- clear signature
	  DEFINE. now  ... ;  -- immediate word

END.  -- optional
```

Notice that word definitions inherit the context and type signature that comes before them.

The same could be writtein in "short form":

```
#: my-library

>: io checker ;
<: default  hello ;

$. name "Dude" ;

?: checker.checksout? ;
!: string -> ;

	: hello "hello" . . ;

?;     -- clear context
!;     -- clear signature

	: now ... ;  -- immediate word

```

It might also be possible to a sectioned form:

```
PROGRAM. pretend-library

IMPORT: io math checker ;

EXPORTS.
: default <- hello ;

STATES.
: name "Dude" ;

WORDS.
  CONTEXT: checker.checksout? ;
  SIGNATURE: string -> ;
  : hello  name "hello" . . ;

  CONTEXT. ;     -- clear context
  SIGNATURE. ;   -- clear signature
	: now  ... ;  -- immediate word

END.
```


## Syntax Option 2

This sytax is more "programmery". Everything is a name and then information that specifices how to define that name. 

A few special names (because they don't actully need names) are `?` and `!` (subject to change if need by),
which define current context and current type signature.

Tentibely arrays are `[ ]` and thunks are `( )`. Words are thunks with a name. They go into the dictionary.
To define a word that returns a thunk use `(( ))` double thunk.

```

my-library (

  io checker  -- namsepace first-class are consumed? bad idea?

  default <- hello ;         -- export

  fooname : "constant" ;     -- constant

  name = "Dude" ;            -- state

  ? ( checker.checksout? )   -- context/guard
  ! [ string -> ]            -- type signature

	hello : ( "hello" . . )    -- word

  ?;     -- clear context
  !;     -- clear signature

	now : ( ... ) ! -- immediate word

)
```

Somtime I think of using `.` instead of `;`? (print `.` would be something else).

Probably each of these have a few kinks to work out.


## Syntax Option 3

YAML.



