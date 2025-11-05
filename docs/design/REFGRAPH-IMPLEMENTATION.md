# Reference Graph Implementation Plan

## Overview

Implementation plan for compile-time reference graph memory management.
This system provides zero-runtime-overhead automatic memory management for nested heap objects by tracking object relationships at compile time and emitting optimal deallocation code.

## Phase 1: Core Data Structures

### RefNode - Heap Object Representation

```c
// Unique identifier for heap objects
typedef uint32_t node_id_t;
#define NODE_ID_INVALID 0

// Reference node - represents a heap-allocated object
typedef struct {
    node_id_t node_id;           // Unique ID for this allocation
    type_id_t object_type;       // TYPE_ARRAY, TYPE_STR, etc.
    bool is_escaped;             // True if ref escapes (FFI, async, etc.)

    // For containers (arrays, records)
    node_id_t* children;         // Array of child node IDs
    size_t child_count;
    size_t child_capacity;
} ref_node_t;
```

### ReferenceGraph - Per-Word Graph State

```c
// Reference graph for a single word
typedef struct {
    // Node storage
    ref_node_t* nodes;           // Array of all nodes
    size_t node_count;
    size_t node_capacity;

    // Next node ID to allocate
    node_id_t next_node_id;

    // Quick lookup: node_id -> array index
    // (Simple array indexed by node_id for now)
    size_t* node_index;          // node_index[node_id] = array index
    size_t node_index_capacity;
} ref_graph_t;
```

### Integration with Type Stack

Augment existing type stack entries:

```c
// Existing structure (from types.h)
typedef struct {
    type_id_t type;
    int slot_id;                 // -1 = not heap-allocated, >=0 = slot index

    // NEW: Reference graph integration
    node_id_t node_id;           // NODE_ID_INVALID if not a heap ref
} type_stack_entry_t;
```

### Compiler Context Extension

```c
// Add to compiler_t structure (in compiler.h)
typedef struct compiler {
    // ... existing fields ...

    // Reference graph for current word
    ref_graph_t* ref_graph;

    // ... rest of fields ...
} compiler_t;
```

## Phase 2: Graph Operations

### Allocation - Create New Node

When compiling array/string literals:

```c
// Create new node in graph
node_id_t ref_graph_alloc_node(ref_graph_t* graph, type_id_t obj_type) {
    // Allocate new node ID
    node_id_t id = graph->next_node_id++;

    // Create node entry
    ref_node_t node = {
        .node_id = id,
        .object_type = obj_type,
        .is_escaped = false,
        .children = NULL,
        .child_count = 0,
        .child_capacity = 0
    };

    // Add to graph
    // ... array growth logic ...
    graph->nodes[graph->node_count++] = node;
    graph->node_index[id] = graph->node_count - 1;

    return id;
}
```

Example usage in compiler:
```c
// Compile: [ 1 2 3 ]
static bool compile_array_literal(compiler_t* comp, ...) {
    // ... parse elements ...

    // Create node for array
    node_id_t array_node = ref_graph_alloc_node(
        comp->ref_graph,
        TYPE_ARRAY
    );

    // Push to type stack
    type_stack_entry_t entry = {
        .type = TYPE_ARRAY,
        .slot_id = allocate_slot(comp),
        .node_id = array_node
    };
    push_type_entry(comp, entry);

    // ... emit allocation code ...
}
```

### Nested Allocation - Parent→Child Edges

When compiling nested structures:

```c
// Add child relationship to node
void ref_graph_add_child(ref_graph_t* graph,
                         node_id_t parent_id,
                         node_id_t child_id) {
    size_t idx = graph->node_index[parent_id];
    ref_node_t* parent = &graph->nodes[idx];

    // Grow children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        parent->children = realloc(parent->children, new_cap * sizeof(node_id_t));
        parent->child_capacity = new_cap;
    }

    // Add child
    parent->children[parent->child_count++] = child_id;
}
```

Example usage:
```c
// Compile: [ [ 1 ] [ 2 ] ]
static bool compile_nested_array(compiler_t* comp, ...) {
    // Create inner arrays
    node_id_t inner1 = ref_graph_alloc_node(comp->ref_graph, TYPE_ARRAY);
    node_id_t inner2 = ref_graph_alloc_node(comp->ref_graph, TYPE_ARRAY);

    // Create outer array
    node_id_t outer = ref_graph_alloc_node(comp->ref_graph, TYPE_ARRAY);

    // Establish relationships
    ref_graph_add_child(comp->ref_graph, outer, inner1);
    ref_graph_add_child(comp->ref_graph, outer, inner2);

    // Push outer to stack
    type_stack_entry_t entry = {
        .type = TYPE_ARRAY,
        .slot_id = allocate_slot(comp),
        .node_id = outer
    };
    push_type_entry(comp, entry);
}
```

### Container Access - Extract Child

When compiling `march.array.at`:

```c
// Extract child reference from parent
static bool compile_array_at(compiler_t* comp) {
    // Pop index
    type_stack_entry_t index_entry = pop_type_entry(comp);

    // Pop array
    type_stack_entry_t array_entry = pop_type_entry(comp);

    if (array_entry.node_id != NODE_ID_INVALID) {
        // Look up parent node
        ref_node_t* parent = ref_graph_get_node(comp->ref_graph, array_entry.node_id);

        // In general case, we don't know which child at compile time
        // So we mark ALL children as potentially accessed
        // (For literal indices, we could be more precise)

        // For now: create a "generic child" node representing "some child of parent"
        // Or: if we know it's index 0 at compile time, use parent->children[0]

        // Push child reference to stack
        // (Simplified - real implementation needs to handle dynamic indices)
    }

    // Check liveness after consuming array reference
    emit_free_dead_nodes(comp);
}
```

### Stack Operations - Aliasing

```c
// Compile: dup
static bool compile_dup(compiler_t* comp) {
    // Get top entry
    type_stack_entry_t top = comp->type_stack[comp->type_stack_depth - 1];

    // Push duplicate (same node_id!)
    push_type_entry(comp, top);

    // NO CHANGE to ref_graph - both stack slots point to same node

    return true;
}

// Compile: drop
static bool compile_drop(compiler_t* comp) {
    // Pop entry
    type_stack_entry_t entry = pop_type_entry(comp);

    // Graph unchanged - node still exists
    // But stack no longer references it

    // Check if node is now unreachable
    emit_free_dead_nodes(comp);

    return true;
}
```

## Phase 3: Liveness Analysis

### Root Set Construction

```c
// Build set of live nodes from current stack state
typedef struct {
    node_id_t* nodes;
    size_t count;
    size_t capacity;
} node_set_t;

node_set_t* build_root_set(compiler_t* comp) {
    node_set_t* roots = node_set_create();

    // Add all nodes referenced by type stack
    for (int i = 0; i < comp->type_stack_depth; i++) {
        node_id_t nid = comp->type_stack[i].node_id;
        if (nid != NODE_ID_INVALID) {
            node_set_add(roots, nid);
        }
    }

    // Add all nodes referenced by live slots (if still using slots)
    // ... slot iteration ...

    // Add all escaped nodes (permanent roots)
    for (size_t i = 0; i < comp->ref_graph->node_count; i++) {
        ref_node_t* node = &comp->ref_graph->nodes[i];
        if (node->is_escaped) {
            node_set_add(roots, node->node_id);
        }
    }

    return roots;
}
```

### Mark Phase - Reachability

```c
// Mark all reachable nodes (recursive DFS)
void mark_reachable(ref_graph_t* graph, node_id_t node_id, bool* marked) {
    if (marked[node_id]) return;  // Already visited

    marked[node_id] = true;

    // Get node
    size_t idx = graph->node_index[node_id];
    ref_node_t* node = &graph->nodes[idx];

    // Recursively mark children
    for (size_t i = 0; i < node->child_count; i++) {
        mark_reachable(graph, node->children[i], marked);
    }
}

// Find all reachable nodes from root set
bool* compute_reachable(compiler_t* comp) {
    size_t max_id = comp->ref_graph->next_node_id;
    bool* marked = calloc(max_id, sizeof(bool));

    // Get root set
    node_set_t* roots = build_root_set(comp);

    // Mark from each root
    for (size_t i = 0; i < roots->count; i++) {
        mark_reachable(comp->ref_graph, roots->nodes[i], marked);
    }

    node_set_free(roots);
    return marked;
}
```

### Sweep Phase - Emit Deallocations

```c
// Emit FREE instructions for dead nodes
void emit_free_dead_nodes(compiler_t* comp) {
    // Compute reachable set
    bool* reachable = compute_reachable(comp);

    // Find dead nodes
    for (size_t i = 0; i < comp->ref_graph->node_count; i++) {
        ref_node_t* node = &comp->ref_graph->nodes[i];

        if (!reachable[node->node_id]) {
            // Node is dead - emit free
            // Must free children first (topological order)
            emit_free_node_recursive(comp, node);
        }
    }

    free(reachable);
}

void emit_free_node_recursive(compiler_t* comp, ref_node_t* node) {
    // Free children first
    for (size_t i = 0; i < node->child_count; i++) {
        node_id_t child_id = node->children[i];
        size_t child_idx = comp->ref_graph->node_index[child_id];
        ref_node_t* child = &comp->ref_graph->nodes[child_idx];
        emit_free_node_recursive(comp, child);
    }

    // Then free this node
    // Emit bytecode: FREE_NODE <node_id>
    // Or if using slots: FREE_SLOT <slot_id>
    cell_buffer_append(comp->cells, encode_free_instruction(node->node_id));

    // Mark as freed (so we don't double-free)
    node->node_id = NODE_ID_INVALID;
}
```

## Phase 4: Deallocation Instructions

### New Bytecode Operation

Add FREE operation to VM:

```c
// In types.h - add new primitive
#define PRIM_FREE_REF  62   /* free-ref - deallocate heap object */
```

```asm
; kernel/x86-64/free-ref.asm
; FREE_REF ( node_ptr -- )
; Deallocates heap object pointed to by TOS

section .text
extern free
extern vm_dispatch
global op_free_ref

op_free_ref:
    ; Get pointer from stack
    mov rax, [rsi]          ; Load pointer to free
    add rsi, 8              ; Pop stack

    ; Check for NULL
    test rax, rax
    jz .skip

    ; Save VM registers
    push rdi
    push rsi

    ; Call free(ptr)
    mov rdi, rax
    mov rbp, rsp
    and rsp, -16
    call free
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

.skip:
    jmp vm_dispatch
```

### Encoding Free Instructions

```c
// Emit FREE for a node
void encode_free_node(compiler_t* comp, node_id_t node_id) {
    // We need the runtime pointer, not the node_id
    // Map node_id -> slot_id -> runtime pointer

    // For now: simple approach
    // Each node has an associated slot that holds its runtime address

    size_t idx = comp->ref_graph->node_index[node_id];
    ref_node_t* node = &comp->ref_graph->nodes[idx];

    // Find type stack entry for this node
    int slot_id = find_slot_for_node(comp, node_id);

    if (slot_id >= 0) {
        // Emit: load slot to stack, then free
        encode_load_slot(comp, slot_id);
        encode_primitive(comp->blob, PRIM_FREE_REF);
    }
}
```

## Phase 5: Integration Points

### Word Compilation Entry Point

```c
bool compiler_compile_word(compiler_t* comp, const char* name, ...) {
    // Initialize reference graph for this word
    comp->ref_graph = ref_graph_create();

    // ... existing compilation ...

    // At word exit: free any remaining unreachable nodes
    emit_free_dead_nodes(comp);

    // Store mini-graph with word's CID (for later JIT/analysis)
    // ... serialization ...

    // Cleanup
    ref_graph_free(comp->ref_graph);
    comp->ref_graph = NULL;
}
```

### Primitive Registration

```c
// Register FREE_REF primitive
REG_PRIM("free-ref", PRIM_FREE_REF, op_free_ref, "ptr ->");
```

## Phase 6: Testing Strategy

### Test 1: Simple Allocation/Deallocation
```march
[ 1 2 3 ]       -- Alloc node A
drop            -- A unreachable → FREE(A)
```

Expected: One allocation, one free emitted.

### Test 2: Aliasing
```march
[ 1 2 3 ]       -- Alloc node A
dup             -- Two refs to A
drop            -- One ref remains
drop            -- Last ref gone → FREE(A)
```

Expected: One allocation, one free (after second drop).

### Test 3: Nested Containers
```march
[ [ 1 ] [ 2 ] ] -- Alloc A, B, C (A→B, A→C)
drop            -- All unreachable → FREE(A), FREE(B), FREE(C)
```

Expected: Three allocations, three frees (children first).

### Test 4: Extraction
```march
[ [ 1 ] [ 2 ] ] -- Alloc A, B, C
0 march.array.at -- Extract B, A consumed
                -- A,C unreachable → FREE(A), FREE(C)
drop            -- B unreachable → FREE(B)
```

Expected: B escapes parent, A and C freed when parent consumed.

### Test 5: Escaped Refs
```march
[ 1 2 3 ]       -- Alloc node A
mark-escaped    -- Mark A as escaped (FFI, etc.)
drop            -- A still escaped, not freed
-- Later: manual free or GC handles it
```

Expected: Allocation, no automatic free.

## Open Issues

### Issue 1: Dynamic Array Indices

When compiling `march.array.at` with runtime index, we don't know which child at compile time.

**Options:**
1. Conservative: Mark ALL children as potentially live
2. Track "unknown child of parent" relationship
3. Create synthetic "child union" node

**Decision:** Start conservative (all children potentially accessed).

### Issue 2: Slot vs Node Lifetime

Slots are SSA-style (last-use tracking). Nodes are reference-counted (multi-use).

**Approach:** Keep both:
- Slots track "where is this value"
- Nodes track "what heap objects exist"
- FREE_REF needs runtime pointer from slot

### Issue 3: Control Flow

Branching creates multiple paths with different liveness.

**Phase 1:** Ignore (straight-line code only)
**Phase 2:** Conservative (union of all paths = live)
**Phase 3:** Flow-sensitive (dead on all paths = free)

## Summary

This implementation provides:
- ✅ Zero runtime overhead (analysis at compile time)
- ✅ Automatic memory management (no manual tracking)
- ✅ Handles aliasing (dup creates multiple refs)
- ✅ Handles nesting (parent→child edges)
- ✅ Safety valve (escaped refs for FFI/async)

Next step: Implement Phase 1 (data structures) and Phase 2 (basic graph operations).
