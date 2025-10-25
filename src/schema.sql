-- March Language Database Schema
-- Stores compiled code, type signatures, and word definitions

-- Type signatures (for type checking)
CREATE TABLE IF NOT EXISTS type_signatures (
    sig_cid BLOB PRIMARY KEY,
    input_sig TEXT,
    output_sig TEXT
);

-- Blobs (compiled code and data, content-addressable by CID)
CREATE TABLE IF NOT EXISTS blobs (
    cid BLOB PRIMARY KEY,
    kind INTEGER NOT NULL,
    sig_cid BLOB,
    flags INTEGER DEFAULT 0,
    len INTEGER NOT NULL,
    data BLOB NOT NULL,
    FOREIGN KEY (sig_cid) REFERENCES type_signatures(sig_cid)
);

-- Word definitions (name -> CID mapping)
CREATE TABLE IF NOT EXISTS words (
    name TEXT NOT NULL,
    namespace TEXT NOT NULL DEFAULT 'user',
    def_cid BLOB,
    type_sig TEXT,
    is_primitive INTEGER DEFAULT 0,
    PRIMARY KEY (name, namespace),
    FOREIGN KEY (def_cid) REFERENCES blobs(cid)
);

-- Definition storage (legacy, may be replaced by blobs)
CREATE TABLE IF NOT EXISTS defs (
    cid BLOB PRIMARY KEY,
    bytecode_version INTEGER DEFAULT 1,
    sig_cid BLOB,
    source_text TEXT,
    source_hash BLOB,
    FOREIGN KEY (sig_cid) REFERENCES type_signatures(sig_cid)
);

-- Create indices for faster lookups
CREATE INDEX IF NOT EXISTS idx_words_name ON words(name);
CREATE INDEX IF NOT EXISTS idx_blobs_cid ON blobs(cid);
