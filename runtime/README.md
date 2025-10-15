# March Runtime

Rust-based runtime components for the March language.

## Components

### State Management (`lib.rs`)

Provides persistent/immutable data structures for March's managed global store.

- Uses `im` crate for efficient persistent data structures
- Exposes C FFI for integration with assembly/C runtime
- Implements structural sharing for memory efficiency

### C API

```c
void march_runtime_init(void);
State* march_state_create(void);
int march_state_get(const State* state, uint64_t key, uint64_t* out_value);
State* march_state_set(const State* state, uint64_t key, uint64_t value);
void march_state_free(State* state);
```

## Building

```bash
cargo build --release
```

Output: `target/release/libmarch_runtime.a` (static library)

## Integration

Link with March VM:
```bash
gcc -o march vm.o primitives.o -L runtime/target/release -lmarch_runtime
```

## Future

This component will eventually be rewritten in March itself once the language is self-hosted.
