/*
 * March Language - Cell Encoding/Decoding
 * Functions for creating and manipulating tagged cells
 */

#ifndef MARCH_CELLS_H
#define MARCH_CELLS_H

#include <stddef.h>
#include "types.h"

/* Create tagged cells */
cell_t encode_xt(void* addr);
cell_t encode_exit(void);
cell_t encode_lit(int64_t value);
cell_t encode_lst(uint64_t sym_id);
cell_t encode_lnt(uint64_t count);

/* Decode cells */
int decode_tag(cell_t cell);
int64_t decode_lit(cell_t cell);
void* decode_xt(cell_t cell);
uint64_t decode_lst(cell_t cell);
uint64_t decode_lnt(cell_t cell);

/* Check cell type */
bool is_exit(cell_t cell);
bool is_lit(cell_t cell);
bool is_xt(cell_t cell);
bool is_lst(cell_t cell);
bool is_lnt(cell_t cell);

/* Cell buffer */
typedef struct {
    cell_t* cells;
    size_t count;
    size_t capacity;
} cell_buffer_t;

cell_buffer_t* cell_buffer_create(void);
void cell_buffer_free(cell_buffer_t* buf);
void cell_buffer_append(cell_buffer_t* buf, cell_t cell);
void cell_buffer_clear(cell_buffer_t* buf);

#endif /* MARCH_CELLS_H */
