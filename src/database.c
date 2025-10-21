/*
 * March Language - SQLite Database Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "database.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

/* Open database */
march_db_t* db_open(const char* filename) {
    march_db_t* db = malloc(sizeof(march_db_t));
    if (!db) return NULL;

    int rc = sqlite3_open(filename, &db->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db->db));
        sqlite3_close(db->db);
        free(db);
        return NULL;
    }

    db->filename = strdup(filename);

    /* Enable foreign keys */
    sqlite3_exec(db->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    return db;
}

/* Close database */
void db_close(march_db_t* db) {
    if (db) {
        sqlite3_close(db->db);
        free(db->filename);
        free(db);
    }
}

/* Initialize schema from SQL file */
bool db_init_schema(march_db_t* db, const char* schema_file) {
    /* Check if schema already exists */
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db,
        "SELECT name FROM sqlite_master WHERE type='table' AND name='words';",
        -1, &stmt, NULL);

    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_ROW) {
            /* Schema already exists */
            return true;
        }
    }

    /* Schema doesn't exist, create it */
    FILE* f = fopen(schema_file, "r");
    if (!f) {
        fprintf(stderr, "Cannot open schema file: %s\n", schema_file);
        return false;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* sql = malloc(size + 1);
    if (!sql) {
        fclose(f);
        return false;
    }

    fread(sql, 1, size, f);
    sql[size] = '\0';
    fclose(f);

    /* Execute SQL */
    char* err_msg = NULL;
    rc = sqlite3_exec(db->db, sql, NULL, NULL, &err_msg);
    free(sql);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

/* Compute SHA256 hash of data */
char* compute_sha256(const uint8_t* data, size_t len) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data, len, hash);

    /* Convert to hex string */
    char* hex = malloc(65);  /* 64 hex chars + null */
    if (!hex) return NULL;

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex + (i * 2), "%02x", hash[i]);
    }
    hex[64] = '\0';

    return hex;
}

/* Store type signature in database (returns sig_cid, caller must free) */
char* db_store_type_sig(march_db_t* db, const char* input_sig, const char* output_sig) {
    if (!db || !output_sig) return NULL;

    /* Default empty input_sig if NULL */
    if (!input_sig) input_sig = "";

    /* Compute sig_cid = SHA256("input_sig|output_sig") */
    size_t sig_str_len = strlen(input_sig) + 1 + strlen(output_sig);
    char* sig_str = malloc(sig_str_len + 1);
    if (!sig_str) return NULL;

    sprintf(sig_str, "%s|%s", input_sig, output_sig);
    char* sig_cid = compute_sha256((uint8_t*)sig_str, sig_str_len);
    free(sig_str);

    if (!sig_cid) return NULL;

    /* Insert into type_signatures (ignore if exists) */
    const char* sql =
        "INSERT OR IGNORE INTO type_signatures (sig_cid, input_sig, output_sig) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare type_sig insert: %s\n", sqlite3_errmsg(db->db));
        free(sig_cid);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, sig_cid, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, input_sig, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, output_sig, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert type_sig: %s\n", sqlite3_errmsg(db->db));
        free(sig_cid);
        return NULL;
    }

    return sig_cid;  /* Caller must free */
}

/* Store blob directly in database (returns cid, caller must free) */
char* db_store_blob(march_db_t* db, int kind, const char* sig_cid,
                    const uint8_t* data, size_t data_len) {
    if (!db || !data) return NULL;

    /* Compute CID */
    char* cid = compute_sha256(data, data_len);
    if (!cid) return NULL;

    /* Insert blob (ignore if exists) */
    const char* sql =
        "INSERT OR IGNORE INTO blobs (cid, kind, sig_cid, flags, len, data) "
        "VALUES (?, ?, ?, 0, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare blob insert: %s\n", sqlite3_errmsg(db->db));
        free(cid);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, kind);
    if (sig_cid) {
        sqlite3_bind_text(stmt, 3, sig_cid, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_int64(stmt, 4, data_len);
    sqlite3_bind_blob(stmt, 5, data, data_len, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert blob: %s\n", sqlite3_errmsg(db->db));
        free(cid);
        return NULL;
    }

    return cid;  /* Caller must free */
}

/* Store compiled word in database */
bool db_store_word(march_db_t* db, const char* name, const char* namespace,
                   const uint8_t* cells, size_t cell_count, const char* type_sig,
                   const char* source_text) {
    /* Compute CID */
    size_t byte_count = cell_count * sizeof(uint64_t);
    char* cid = compute_sha256(cells, byte_count);
    if (!cid) return false;

    /* Parse type signature into input/output parts */
    char* sig_cid = NULL;
    if (type_sig && strlen(type_sig) > 0) {
        /* Parse "input -> output" or "-> output" */
        const char* arrow = strstr(type_sig, "->");
        char input_sig[256] = "";
        char output_sig[256] = "";

        if (arrow) {
            /* Extract input (everything before "->") */
            size_t input_len = arrow - type_sig;
            if (input_len > 0) {
                strncpy(input_sig, type_sig, input_len);
                input_sig[input_len] = '\0';
                /* Trim trailing whitespace */
                while (input_len > 0 && input_sig[input_len - 1] == ' ') {
                    input_sig[--input_len] = '\0';
                }
            }

            /* Extract output (everything after "->") */
            const char* output_start = arrow + 2;
            while (*output_start == ' ') output_start++;  /* Skip leading spaces */
            strcpy(output_sig, output_start);
            /* Trim trailing whitespace */
            size_t output_len = strlen(output_sig);
            while (output_len > 0 && output_sig[output_len - 1] == ' ') {
                output_sig[--output_len] = '\0';
            }
        }

        /* Store type signature and get sig_cid */
        sig_cid = db_store_type_sig(db, input_sig[0] ? input_sig : NULL, output_sig);
        if (!sig_cid) {
            fprintf(stderr, "Failed to store type signature\n");
            free(cid);
            return false;
        }
    }

    /* Compute source hash if source text provided */
    char* source_hash = NULL;
    if (source_text) {
        source_hash = compute_sha256((uint8_t*)source_text, strlen(source_text));
    }

    /* Start transaction */
    sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    /* Insert blob (ignore if exists) */
    const char* blob_sql =
        "INSERT OR IGNORE INTO blobs (cid, kind, sig_cid, flags, len, data) "
        "VALUES (?, ?, ?, 0, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, blob_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare blob insert: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        if (sig_cid) free(sig_cid);
        if (source_hash) free(source_hash);
        return false;
    }

    sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, BLOB_CODE);
    if (sig_cid) {
        sqlite3_bind_text(stmt, 3, sig_cid, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_int64(stmt, 4, byte_count);
    sqlite3_bind_blob(stmt, 5, cells, byte_count, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert blob: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        if (sig_cid) free(sig_cid);
        if (source_hash) free(source_hash);
        return false;
    }

    /* Insert or replace word (handles recompilation of same word) */
    const char* word_sql =
        "INSERT OR REPLACE INTO words (name, namespace, def_cid, type_sig, is_primitive) "
        "VALUES (?, ?, ?, ?, 0);";

    rc = sqlite3_prepare_v2(db->db, word_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare word insert: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        if (sig_cid) free(sig_cid);
        if (source_hash) free(source_hash);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, namespace ? namespace : "user", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, cid, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, type_sig, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert word: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        if (sig_cid) free(sig_cid);
        if (source_hash) free(source_hash);
        return false;
    }

    /* Insert or replace defs entry with source text */
    if (source_text) {
        const char* defs_sql =
            "INSERT OR REPLACE INTO defs "
            "(cid, bytecode_version, sig_cid, source_text, source_hash) "
            "VALUES (?, 1, ?, ?, ?);";

        rc = sqlite3_prepare_v2(db->db, defs_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare defs insert: %s\n", sqlite3_errmsg(db->db));
            sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
            free(cid);
            if (sig_cid) free(sig_cid);
            if (source_hash) free(source_hash);
            return false;
        }

        sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);
        if (sig_cid) {
            sqlite3_bind_text(stmt, 2, sig_cid, -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_text(stmt, 3, source_text, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, source_hash, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Failed to insert defs: %s\n", sqlite3_errmsg(db->db));
            sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
            free(cid);
            if (sig_cid) free(sig_cid);
            if (source_hash) free(source_hash);
            return false;
        }
    }

    /* Commit transaction */
    sqlite3_exec(db->db, "COMMIT;", NULL, NULL, NULL);
    free(cid);
    if (sig_cid) free(sig_cid);
    if (source_hash) free(source_hash);
    return true;
}

/* Load word by name */
uint64_t* db_load_word(march_db_t* db, const char* name, const char* namespace,
                       size_t* cell_count) {
    const char* sql =
        "SELECT b.data, b.len "
        "FROM words w "
        "JOIN blobs b ON w.def_cid = b.cid "
        "WHERE w.name = ? AND w.namespace = ?;";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare load query: %s\n", sqlite3_errmsg(db->db));
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, namespace ? namespace : "user", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "Word not found: %s:%s\n", namespace ? namespace : "user", name);
        sqlite3_finalize(stmt);
        return NULL;
    }

    /* Get blob data */
    const void* blob = sqlite3_column_blob(stmt, 0);
    int len = sqlite3_column_int(stmt, 1);

    if (len % sizeof(uint64_t) != 0) {
        fprintf(stderr, "Invalid blob size: %d\n", len);
        sqlite3_finalize(stmt);
        return NULL;
    }

    *cell_count = len / sizeof(uint64_t);

    /* Allocate and copy */
    uint64_t* cells = malloc(len);
    if (!cells) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    memcpy(cells, blob, len);
    sqlite3_finalize(stmt);

    return cells;
}

/* ============================================================================ */
/* CID-based storage functions (LINKING.md design) */
/* ============================================================================ */

/* Store literal data, return CID (caller must free) */
char* db_store_literal(march_db_t* db, int64_t value, const char* type_sig) {
    if (!db) return NULL;

    /* Serialize value as little-endian int64 */
    uint8_t data[8];
    for (int i = 0; i < 8; i++) {
        data[i] = (value >> (i * 8)) & 0xFF;
    }

    /* Store type signature if provided */
    char* sig_cid = NULL;
    if (type_sig) {
        sig_cid = db_store_type_sig(db, NULL, type_sig);
    }

    /* Store as BLOB_DATA */
    char* cid = db_store_blob(db, BLOB_DATA, sig_cid, data, 8);

    if (sig_cid) free(sig_cid);
    return cid;
}

/* Load any blob by CID */
bool db_load_blob_ex(march_db_t* db, const char* cid,
                     int* kind, char** sig_cid,
                     uint8_t** data, size_t* data_len) {
    if (!db || !cid) return false;

    const char* sql =
        "SELECT kind, sig_cid, data, len FROM blobs WHERE cid = ?;";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare blob load: %s\n", sqlite3_errmsg(db->db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }

    /* Get blob metadata */
    if (kind) {
        *kind = sqlite3_column_int(stmt, 0);
    }

    if (sig_cid) {
        const char* sig = (const char*)sqlite3_column_text(stmt, 1);
        *sig_cid = sig ? strdup(sig) : NULL;
    }

    /* Get blob data */
    const void* blob = sqlite3_column_blob(stmt, 2);
    int len = sqlite3_column_int(stmt, 3);

    if (data && data_len) {
        *data_len = len;
        *data = malloc(len);
        if (*data) {
            memcpy(*data, blob, len);
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

/* Get just the blob kind (fast lookup) */
int db_get_blob_kind(march_db_t* db, const char* cid) {
    if (!db || !cid) return -1;

    const char* sql = "SELECT kind FROM blobs WHERE cid = ?;";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int kind = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return kind;
}
