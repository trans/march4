-- March Language Database Schema
-- Content-Addressable Store for Code, Definitions, and Data
-- Author: Thomas Sawyer & AI
-- Date: 2025-10-14

-- Enable foreign keys
PRAGMA foreign_keys = ON;

-- ============================================================================
-- BLOBS: Content-addressable binary data
-- ============================================================================
-- Stores all content-addressable data: compiled code streams, serialized
-- literals, heap object snapshots, etc.
-- The CID (Content ID) is a SHA256 hash of the blob content.

CREATE TABLE blobs (
    cid         TEXT PRIMARY KEY,  -- SHA256 hash of content (hex encoded)
    kind        INTEGER NOT NULL,  -- Type/format identifier
    flags       INTEGER NOT NULL DEFAULT 0,  -- Metadata flags
    len         INTEGER NOT NULL,  -- Length in bytes
    data        BLOB NOT NULL,     -- The actual binary content
    created_at  INTEGER NOT NULL DEFAULT (unixepoch()),  -- Timestamp

    CHECK (length(cid) = 64),      -- SHA256 is 64 hex chars
    CHECK (len = length(data))     -- Enforce consistency
);

CREATE INDEX idx_blobs_kind ON blobs(kind);
CREATE INDEX idx_blobs_created ON blobs(created_at);


-- ============================================================================
-- WORDS: Named definitions in the dictionary
-- ============================================================================
-- Maps human-readable names to their definitions (CIDs).
-- Supports namespacing, versioning, and multiple definitions per name
-- (for overloading/multi-dispatch resolved at compile time).

CREATE TABLE words (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,         -- Word name (e.g., "hello", "+", "dup")
    namespace   TEXT DEFAULT 'user',   -- Namespace/module
    def_cid     TEXT NOT NULL,         -- CID of definition (references blobs)
    type_sig    TEXT,                  -- Type signature (e.g., "int int -> int")
    is_primitive INTEGER NOT NULL DEFAULT 0,  -- 1 if machine code primitive, 0 if user word
    architecture TEXT,                 -- CPU arch (e.g., "x86-64", "aarch64") for primitives
    is_immediate INTEGER NOT NULL DEFAULT 0,  -- 1 if immediate/compile-time word
    doc         TEXT,                  -- Documentation string
    created_at  INTEGER NOT NULL DEFAULT (unixepoch()),

    FOREIGN KEY (def_cid) REFERENCES blobs(cid) ON DELETE RESTRICT,
    CHECK (is_primitive IN (0, 1)),
    CHECK (is_immediate IN (0, 1)),
    CHECK ((is_primitive = 0 AND architecture IS NULL) OR
           (is_primitive = 1 AND architecture IS NOT NULL))
);

-- Efficient lookups by name and namespace
CREATE INDEX idx_words_name ON words(name, namespace);
CREATE INDEX idx_words_namespace ON words(namespace);
CREATE INDEX idx_words_def_cid ON words(def_cid);
CREATE INDEX idx_words_type ON words(type_sig);


-- ============================================================================
-- DEFS: Compiled code and metadata
-- ============================================================================
-- Additional metadata about compiled definitions beyond just the blob.
-- Links CIDs to their constituent parts and compilation info.

CREATE TABLE defs (
    cid             TEXT PRIMARY KEY,  -- CID of this definition (references blobs)
    bytecode_version INTEGER NOT NULL DEFAULT 1,  -- Bytecode format version
    stack_effect    TEXT,              -- Stack effect notation (e.g., "( a b -- c )")
    is_pure         INTEGER NOT NULL DEFAULT 0,   -- 1 if side-effect free
    escapes         INTEGER NOT NULL DEFAULT 0,   -- 1 if captures/escapes values
    source_text     TEXT,              -- Original source code (optional)
    source_hash     TEXT,              -- Hash of source (for change detection)
    compiled_at     INTEGER NOT NULL DEFAULT (unixepoch()),

    FOREIGN KEY (cid) REFERENCES blobs(cid) ON DELETE CASCADE,
    CHECK (is_pure IN (0, 1)),
    CHECK (escapes IN (0, 1))
);


-- ============================================================================
-- EDGES: Dependency graph for GC and analysis
-- ============================================================================
-- Tracks references between objects for garbage collection and dead code
-- elimination. An edge from A to B means "A references B".

CREATE TABLE edges (
    from_cid    TEXT NOT NULL,     -- Source CID
    to_cid      TEXT NOT NULL,     -- Referenced CID
    edge_type   TEXT NOT NULL,     -- Type of reference: 'call', 'literal', 'data'

    FOREIGN KEY (from_cid) REFERENCES blobs(cid) ON DELETE CASCADE,
    FOREIGN KEY (to_cid) REFERENCES blobs(cid) ON DELETE RESTRICT,

    PRIMARY KEY (from_cid, to_cid, edge_type)
);

CREATE INDEX idx_edges_from ON edges(from_cid);
CREATE INDEX idx_edges_to ON edges(to_cid);


-- ============================================================================
-- MODULES: Program/library organization
-- ============================================================================
-- Represents a compilation unit (program or library).
-- Contains manifest of exported words and dependencies.

CREATE TABLE modules (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,  -- Module name
    manifest_cid TEXT NOT NULL,        -- CID of module manifest (JSON/binary)
    version     TEXT,                  -- Semantic version
    is_root     INTEGER NOT NULL DEFAULT 0,  -- 1 if GC root
    created_at  INTEGER NOT NULL DEFAULT (unixepoch()),

    FOREIGN KEY (manifest_cid) REFERENCES blobs(cid) ON DELETE RESTRICT,
    CHECK (is_root IN (0, 1))
);

CREATE INDEX idx_modules_name ON modules(name);
CREATE INDEX idx_modules_manifest ON modules(manifest_cid);


-- ============================================================================
-- MODULE_EXPORTS: What each module exports
-- ============================================================================
-- Maps module exports to word definitions.

CREATE TABLE module_exports (
    module_id   INTEGER NOT NULL,
    export_name TEXT NOT NULL,     -- Exported name (may differ from word name)
    word_id     INTEGER NOT NULL,  -- Reference to words table

    FOREIGN KEY (module_id) REFERENCES modules(id) ON DELETE CASCADE,
    FOREIGN KEY (word_id) REFERENCES words(id) ON DELETE RESTRICT,

    PRIMARY KEY (module_id, export_name)
);

CREATE INDEX idx_module_exports_word ON module_exports(word_id);


-- ============================================================================
-- MODULE_IMPORTS: Module dependencies
-- ============================================================================
-- Tracks which modules import which other modules.

CREATE TABLE module_imports (
    importer_id INTEGER NOT NULL,  -- Module doing the importing
    imported_id INTEGER NOT NULL,  -- Module being imported
    import_alias TEXT,             -- Optional alias

    FOREIGN KEY (importer_id) REFERENCES modules(id) ON DELETE CASCADE,
    FOREIGN KEY (imported_id) REFERENCES modules(id) ON DELETE RESTRICT,

    PRIMARY KEY (importer_id, imported_id)
);


-- ============================================================================
-- STATE: Global mutable state (variables)
-- ============================================================================
-- Stores global state variables. Values are immutable snapshots (CIDs).
-- Updates create new entries with new CIDs.

CREATE TABLE state (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,
    namespace   TEXT DEFAULT 'user',
    value_cid   TEXT NOT NULL,     -- CID of current value (frozen snapshot)
    type_sig    TEXT,              -- Type of the value
    updated_at  INTEGER NOT NULL DEFAULT (unixepoch()),

    FOREIGN KEY (value_cid) REFERENCES blobs(cid) ON DELETE RESTRICT,
    UNIQUE (name, namespace)
);

CREATE INDEX idx_state_name ON state(name, namespace);


-- ============================================================================
-- STATE_HISTORY: Audit trail for state changes
-- ============================================================================
-- Optional: tracks historical values of state variables.

CREATE TABLE state_history (
    state_id    INTEGER NOT NULL,
    value_cid   TEXT NOT NULL,
    changed_at  INTEGER NOT NULL DEFAULT (unixepoch()),

    FOREIGN KEY (state_id) REFERENCES state(id) ON DELETE CASCADE,
    FOREIGN KEY (value_cid) REFERENCES blobs(cid) ON DELETE RESTRICT
);

CREATE INDEX idx_state_history_state ON state_history(state_id);
CREATE INDEX idx_state_history_changed ON state_history(changed_at);


-- ============================================================================
-- METADATA: System configuration and versioning
-- ============================================================================
-- Stores schema version and other system-level metadata.

CREATE TABLE metadata (
    key     TEXT PRIMARY KEY,
    value   TEXT NOT NULL
);

-- Initialize with schema version
INSERT INTO metadata (key, value) VALUES ('schema_version', '1');
INSERT INTO metadata (key, value) VALUES ('created_at', unixepoch());
INSERT INTO metadata (key, value) VALUES ('march_version', 'α₄');


-- ============================================================================
-- VIEWS: Convenient queries
-- ============================================================================

-- All words with their definition info
CREATE VIEW v_words_full AS
SELECT
    w.id,
    w.name,
    w.namespace,
    w.type_sig,
    w.is_primitive,
    w.architecture,
    w.is_immediate,
    w.doc,
    w.def_cid,
    b.len as code_len,
    d.bytecode_version,
    d.stack_effect,
    d.is_pure,
    d.source_text
FROM words w
JOIN blobs b ON w.def_cid = b.cid
LEFT JOIN defs d ON w.def_cid = d.cid;


-- Module dependency graph
CREATE VIEW v_module_deps AS
SELECT
    m1.name as importer,
    m2.name as imported,
    mi.import_alias
FROM module_imports mi
JOIN modules m1 ON mi.importer_id = m1.id
JOIN modules m2 ON mi.imported_id = m2.id;


-- Current state snapshot
CREATE VIEW v_current_state AS
SELECT
    s.name,
    s.namespace,
    s.type_sig,
    s.value_cid,
    s.updated_at,
    b.len as value_len
FROM state s
JOIN blobs b ON s.value_cid = b.cid;
