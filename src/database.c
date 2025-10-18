/*
 * March Language - SQLite Database Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "database.h"
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
    int rc = sqlite3_exec(db->db, sql, NULL, NULL, &err_msg);
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

/* Store compiled word in database */
bool db_store_word(march_db_t* db, const char* name, const char* namespace,
                   const uint8_t* cells, size_t cell_count, const char* type_sig) {
    /* Compute CID */
    size_t byte_count = cell_count * sizeof(uint64_t);
    char* cid = compute_sha256(cells, byte_count);
    if (!cid) return false;

    /* Start transaction */
    sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    /* Insert blob (ignore if exists) */
    const char* blob_sql =
        "INSERT OR IGNORE INTO blobs (cid, kind, flags, len, data) "
        "VALUES (?, 1, 0, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, blob_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare blob insert: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        return false;
    }

    sqlite3_bind_text(stmt, 1, cid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, byte_count);
    sqlite3_bind_blob(stmt, 3, cells, byte_count, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert blob: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
        return false;
    }

    /* Insert word */
    const char* word_sql =
        "INSERT INTO words (name, namespace, def_cid, type_sig, is_primitive) "
        "VALUES (?, ?, ?, ?, 0);";

    rc = sqlite3_prepare_v2(db->db, word_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare word insert: %s\n", sqlite3_errmsg(db->db));
        sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, NULL);
        free(cid);
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
        return false;
    }

    /* Commit transaction */
    sqlite3_exec(db->db, "COMMIT;", NULL, NULL, NULL);
    free(cid);
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
