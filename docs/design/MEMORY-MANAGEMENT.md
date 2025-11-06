# Reference Graph Memory Management - Design Document

## Overview
Compile-time reference graph analysis with compile-time (and JIT-time, if neccessary) deallocation decisions. Zero runtime overhead for memory management by tracking object relationships during compilation and emitting optimal deallocation code during compile-time (or JIT specialization).

## Core Concepts

### Reference Graph
A directed graph tracking heap-allocated objects and their relationships:
- **Nodes**: Heap objects (arrays, records, etc.)
- **Edges**: Containment relationships (parent → children)
- **References**: Stack positions and slots that point to nodes

### Mini-Graphs
Each word has a mini-graph stored with its CID:
- Input stack types (may contain type holes)
- Output stack types (may contain type holes)
- Graph structure showing object creation and relationships
- Operations performed on references

### Type Holes
Polymorphic types to be filled at JIT time:
- Allow words to work with multiple concrete types
- Filled by JIT based on actual stack types at call site
- Enable connection between mini-graphs at word boundaries

## Data Structures

### RefNode (Compile-Time Type)
RefNode contains:
  - node_id: unique identifier
  - object_type: underlying type (Array<T>, Record, etc.)
  - children: set of child node_ids (for containers)

### ReferenceGraph (Per-Word)
ReferenceGraph contains:
  - nodes: map of node_id → node metadata
  - edges: map of parent_id → set of child_ids
  - input_stack: list of RefNode types (may have holes)
  - output_stack: list of RefNode types (may have holes)

### Stack State (During Compilation/JIT)
Stack contains:
  - slots: list of RefNode references
  - Note: Multiple slots may reference same node_id (aliasing from dup)

## Compilation Process

### Phase 1: Compile-Time (Per Word)

For each word being compiled:

1. **Initialize**: Create empty ReferenceGraph with input stack signature
2. **Process instructions**:
   - **Allocation** (array/record literals): Create new node, add to graph
   - **dup**: Add another reference to top stack node (same node_id)
   - **drop**: Remove reference from stack (don't deallocate yet)
   - **Container access** (at, etc.): Create edge parent → child in graph, push child reference to stack
   - **Arithmetic/logic**: Consume/produce non-ref values (no graph changes)
3. **Finalize**: Record output stack signature
4. **Store**: Save mini-graph with word's CID

### Phase 2: JIT-Time (At Call Site)

When specializing a word with concrete types:

1. **Load mini-graph** from CID
2. **Fill type holes** using concrete types from runtime stack
3. **Instantiate graph** with actual types
4. **Stitch with prior word**: Connect previous output to this input
5. **Liveness analysis**: Determine which nodes are unreachable
6. **Emit deallocation code** at appropriate points
7. **Cache**: Store compiled code indexed by (CID, type-signature)

## Liveness Analysis Algorithm

At each potential deallocation point (typically when refs are consumed):

Step 1 - Build root set:
   - All RefNodes currently on stack
   - All RefNodes in live slots (if using slots)

Step 2 - Mark phase:
   - Starting from roots, recursively mark all reachable nodes
   - Follow edges: if parent is marked, mark all children

Step 3 - Sweep phase:
   - Any unmarked nodes are unreachable
   - Safe to deallocate these nodes

Step 4 - Emit deallocation:
   - Generate code to free unmarked nodes
   - Must respect parent-child order (children first, then parents)

## Key Operations

### Object Allocation
Example: [ [ 1 ] [ 2 ] ]  (outer array literal)

Graph operations:
1. Create node B for [1]
2. Create node C for [2]
3. Create node A for outer array
4. Add edges: A → B, A → C
5. Push RefNode(A) to stack

### Container Access
Example: array 2 at  (extract element at index 2)

Graph operations:
1. Pop RefNode(A) from stack (array)
2. Pop index from stack
3. Look up A's children in graph
4. Push RefNode(C) to stack (child at index 2)
5. A is consumed - check liveness:
   - If C is only remaining reference to A's subgraph: emit dealloc(A, B)
   - C stays live (on stack)

### Duplication
Example: dup

Graph operations:
1. Peek top stack → RefNode(A, node_id=123)
2. Push another RefNode(A, node_id=123)
3. Graph unchanged (same node, multiple references)

### Drop
Example: drop

Graph operations:
1. Pop RefNode(A) from stack
2. Check liveness:
   - Build root set from remaining stack
   - If A is unreachable: emit deallocation for A and unreachable children
   - If A is still reachable (aliased): do nothing

## Cross-Word Interaction

### Word Composition
Example scenario:
  word1 : [ [ 1 ] ] ;        (returns Array[Array[Int]])
  word2 : word1 0 at ;       (calls word1, extracts child)

JIT process:
1. Compile word1:
   - Mini-graph: creates nodes A, B with edge A → B
   - Output: [RefNode(A)]

2. Compile word2:
   - Input expects: [RefNode(array_type)]
   - Calls word1 (inline or emit call)
   - Mini-graph: extracts child from input
   - Output: [RefNode(B)]

3. Stitch:
   - word1's output A matches word2's input requirement
   - Combine graphs: A → B relationship preserved
   - After 'at': A is consumed, B is returned
   - Emit: deallocate A (B is still live)

## Immediate Words

Immediate words execute at compile-time and manipulate the graph.

Example: IMMEDIATE dup
  Compile-time graph update:
    stack.push(stack.top())  (duplicate top reference)

  Emit runtime code:
    emit(DUP_INSTRUCTION)    (actual runtime dup operation)

Common immediate words that affect the graph:
- dup, drop, swap, over, rot - stack manipulation
- at, set-at - container access
- Potentially arithmetic if it consumes refs

## Implementation Steps

### Step 1: Basic Infrastructure
- Define RefNode type
- Define ReferenceGraph structure
- Add graph field to word compilation context
- Modify type stack to track RefNodes

### Step 2: Graph Building
- Allocation: array/record literals create nodes
- Container access: create edges and extract children
- Stack operations: track aliasing (dup) and removal (drop)
- Store mini-graphs with word CIDs

### Step 3: Liveness Analysis
- Implement mark-and-sweep from root set
- Identify deallocation points (last use of refs)
- Handle circular references correctly

### Step 4: JIT Integration
- Load mini-graphs from CID
- Fill type holes with concrete types
- Stitch graphs at word boundaries
- Emit deallocation code in JIT output
- Cache compiled versions by (CID, types)

(IMPORTANT! Further analysis reveals that it might not matter the concrete types, and all of it can be done at compile time instead.)

### Step 5: Testing
- Simple allocation/deallocation
- Nested containers (arrays of arrays)
- Aliasing (dup behavior)
- Extraction (container access)
- Cross-word composition
- Circular references

## Edge Cases

### Circular References
Example: Two arrays pointing to each other
  a b  a b 0 set-at  b a 0 set-at

Graph: A ⇄ B (bidirectional edges)
Deallocation: Both must be unreachable to deallocate either

### Multiple Extractions
Example: [ [ 1 ] ] dup 0 at swap 0 at

Stack progression:
[A] → [A A] → [A B] → [B A] → [B A]

B is extracted twice but same node_id
Both references to A are consumed
Deallocate A when last ref drops (B stays live)

### Escaped But Dropped
Example: [ [ 1 ] ] 0 at drop

Extract child, then immediately drop it
Liveness analysis: both A and B unreachable
Deallocate both

## Performance Considerations

- **Compile-time cost**: Graph building is cheap (pointer manipulation)
- **JIT-time cost**: One-time liveness analysis per specialization
- **Runtime cost**: Zero - just normal deallocation instructions
- **Cache effectiveness**: Same (CID, types) reuses compiled code

## Benefits

1. **No runtime overhead**: Analysis happens at compile/JIT time
2. **Deterministic**: Same code always produces same deallocation pattern
3. **Handles aliasing**: Multiple refs to same object tracked correctly
4. **Handles nesting**: Parent-child relationships preserved
5. **Handles cycles**: Standard reachability handles circular refs
6. **No manual management**: Programmer doesn't track lifetimes

## Open Questions

- **Slots**: If keeping slot-based storage, how do slots interact with graph?
- **FFI**: How do foreign function calls affect graph? (probably assume they don't return refs)
- **Polymorphism**: How many type specializations to cache? (limit per word?)
- **Debugging**: How to visualize graphs for debugging compiler issues?


## Additional Considerations

4. A few small improvements you might add before handing to Codex

Just to make the document “implementation ready”:

1. Explicit notion of “escape”

Add a bullet in Key Operations:

> Escape (for FFI, closures):
> Mark the node as escaped; treat it as a permanent root. Escaped nodes are never auto-deallocated by the graph-based liveness pass.
> Might not need this is we enfore copying only VIA FFI and others?

2. Control-flow section

Short subsection:

> Control flow inside words:
> ReferenceGraph is maintained per basic block; stack and graph states are propagated along edges. At merges, we intersect liveness and union edges.
> Deallocation is only emitted where a node is dead on all outgoing paths.

3. Ownership summary per word

Under “Cross-Word Interaction”, add:

> For each input slot, we compute whether the word consumes, retains, or escapes the referenced node.
> This summary is stored alongside the mini-graph and used at JIT-time to stitch ownership correctly across calls.
> Do we really need this? If we can do it all at compile time, we certainly dodn't need to pass ti along to the JIT.

4. Fallback / conservative mode

Any node whose lifetime cannot be proven finite by the analysis (due to escape, unknown side effects, or incomplete information) is marked as non-managed by this system and must be freed via other mechanisms.


## Yes, you can do the liveness & free placement at compile time

For the current March VM (bytecode interpreter), you can absolutely:

1. Build the ReferenceGraph while compiling a word

You’re already:

- tracking types on the stack,
- building the mini-graph (nodes, edges, aliases).

2. Run liveness analysis over the word’s bytecode (still at compile time)

- Treat roots as:
  - stack slots at each program point,
  -  plus any explicit “escape sites” you decide on.
- Compute:
  - where each node’s last use is in that word,
  - which nodes are still live at each exit / branch.

3. Insert explicit free operations into the bytecode

Example:

... bytecode ...
DROP            ; last use of node 17
FREE_NODE 17    ; compile-time-inserted
...

Or better: `FREE_SLOT i` if you want to refer to “the object that was in slot i at this point” instead of a global node id — whichever matches the VM architecture.

4. At runtime, the interpreter just executes FREE_* like any other op:

- No analysis
- Just “call the allocator’s free on this handle”.

In other words: the algorithm described under “Phase 2: JIT-Time” can be run on the word’s bytecode at compile time just as well.
The JIT is only a consumer of the resulting annotated bytecode.

Where a later JIT comes in is optional sugar:

- If you JIT a word later:
  - You just treat ALLOC / FREE_NODE as normal ops and inline the calls.
  - Or even “erase” the explicit FREE_NODE and replace them with direct free() calls in the native code.

So yes, practically: you can ship your VM with compile-time graph + compile-time liveness + explicit FREE bytecodes, no JIT necessary.


## How this plugs into the existing SSA / slot scheme

Do we replace the slot system or augment it?

Right now (roughly):

- Each instruction writes to a fresh SSA-like slot (or updates a stack index),
- You know, for each slot, where its last use is,
- For simple scalars and single-level refs, you can already emit frees at the last use and be done.

Where it breaks down with nested refs is:

- A single “value” in one slot might represent an entire tree of heap objects:
  - Outer array
  - Inner arrays
  - Records inside those, etc.
- Your current model probably only has “one ref” per slot, so it doesn’t understand the children.

The trick is: don’t throw away what you already have. Just layer a RefNode graph over it:

- Each SSA slot continues to carry:
  - its type,
  - plus (optionally) a node_id (or a set of node_ids for the more conservative case).
- The ReferenceGraph lives alongside the SSA state and describes:
  - which heap objects exist,
  - how they’re related (parent → children).

Then your SSA liveness gives you “when is this slot last read?” and the graph gives you “what subtree does that slot still keep alive?”.

When a slot drops or is overwritten:

1. You remove that ref from the root set.
2. Run (or reuse incremental) reachability on the graph.
3. Any nodes now unreachable → emit frees (children first, then parents).

So the change isn’t “replace SSA with graphs”, it’s “augment SSA slots with node-graph identities”.

(Comment: Maybe though slots are no longer needed, but this might be good way to transition.)


## Takeaway

This design enables zero-overhead memory management through compile-time analysis and JIT-time code generation, avoiding runtime reference counting or garbage collection.
