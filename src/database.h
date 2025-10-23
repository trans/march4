/*
 * March Language - SQLite Database Integration
 */

#ifndef MARCH_DATABASE_H
#define MARCH_DATABASE_H

#include <sqlite3.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Database handle */
typedef struct {
    sqlite3* db;
    char* filename;
} march_db_t;

/* Open/close database */
march_db_t* db_open(const char* filename);
void db_close(march_db_t* db);

/* Initialize schema if needed */
bool db_init_schema(march_db_t* db, const char* schema_file);

/* Store type signature (returns binary sig_cid, caller must free) */
unsigned char* db_store_type_sig(march_db_t* db, const char* input_sig, const char* output_sig);

/* Store blob directly (returns binary cid, caller must free) */
unsigned char* db_store_blob(march_db_t* db, int kind, const unsigned char* sig_cid,
                              const uint8_t* data, size_t data_len);

/* Store compiled word */
bool db_store_word(march_db_t* db, const char* name, const char* namespace,
                   const uint8_t* cells, size_t cell_count, const char* type_sig,
                   const char* source_text);

/* Load word by name */
uint64_t* db_load_word(march_db_t* db, const char* name, const char* namespace,
                       size_t* cell_count);

/* Compute SHA256 CID (returns 32-byte binary hash, caller must free) */
unsigned char* compute_sha256(const uint8_t* data, size_t len);

/* Convert binary CID to hex string for display (caller must free) */
char* cid_to_hex(const unsigned char* cid);

/* ============================================================================ */
/* CID-based storage functions (LINKING.md design) */
/* ============================================================================ */

/* Store literal data, return binary CID (caller must free) */
unsigned char* db_store_literal(march_db_t* db, int64_t value, const char* type_sig);

/* Load any blob by CID
 * Returns: true if found
 * Outputs:
 *   kind: blob kind (BLOB_PRIMITIVE, BLOB_WORD, etc.)
 *   sig_cid: signature CID (caller must free if non-NULL)
 *   data: blob data (caller must free if non-NULL)
 *   data_len: length of data
 */
bool db_load_blob_ex(march_db_t* db, const unsigned char* cid,
                     int* kind, unsigned char** sig_cid,
                     uint8_t** data, size_t* data_len);

/* Get just the blob kind (fast lookup) */
int db_get_blob_kind(march_db_t* db, const unsigned char* cid);

#endif /* MARCH_DATABASE_H */
