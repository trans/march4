# March Bootstrap Compiler

OCaml-based outer compiler for the March language.

## Purpose

The bootstrap compiler reads March source code and generates:
- Cell streams (tagged bytecode)
- Content-addressable objects (CIDs)
- SQLite database entries

## Status

**TODO**: Not yet implemented

## Planned Components

- **Lexer** - Tokenize source text
- **Parser** - Build AST from tokens
- **Type Checker** - Static type checking with shadow type stack
- **Resolver** - Resolve names to CIDs
- **Code Generator** - Emit cell streams
- **Database Writer** - Store to SQLite with CAS

## Why OCaml?

- Built-in persistent data structures (perfect for compiler work)
- Pattern matching for AST manipulation
- Fast native compilation
- Excellent for building compilers
- No runtime dependency in March binaries (compiler is just tooling)

## Future

Once March is self-hosted, this bootstrap compiler can be replaced with a March-native compiler.
