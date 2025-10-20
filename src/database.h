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

/* Store type signature (returns sig_cid, caller must free) */
char* db_store_type_sig(march_db_t* db, const char* input_sig, const char* output_sig);

/* Store compiled word */
bool db_store_word(march_db_t* db, const char* name, const char* namespace,
                   const uint8_t* cells, size_t cell_count, const char* type_sig,
                   const char* source_text);

/* Load word by name */
uint64_t* db_load_word(march_db_t* db, const char* name, const char* namespace,
                       size_t* cell_count);

/* Compute SHA256 CID */
char* compute_sha256(const uint8_t* data, size_t len);

#endif /* MARCH_DATABASE_H */
