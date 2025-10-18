/*
 * March Language - Cell Encoding/Decoding Implementation
 */

#include "cells.h"
#include <stdlib.h>
#include <string.h>

/* Encode XT (execute word) - clear lower 2 bits, set tag 00 */
cell_t encode_xt(void* addr) {
    return ((uint64_t)addr & ~0x3ULL) | TAG_XT;
}

/* Encode EXIT - XT with address 0 */
cell_t encode_exit(void) {
    return 0ULL;
}

/* Encode LIT - embed 62-bit signed value, tag 01 */
cell_t encode_lit(int64_t value) {
    return ((uint64_t)value << 2) | TAG_LIT;
}

/* Encode LST (symbol literal) - embed symbol ID, tag 010 (3-bit) */
cell_t encode_lst(uint64_t sym_id) {
    return (sym_id << 3) | TAG_LST;  /* Shift 3 bits for 3-bit tag */
}

/* Encode LNT (bulk literals) - embed count, tag 110 */
cell_t encode_lnt(uint64_t count) {
    return (count << 3) | TAG_LNT;
}

/* Decode tag from cell */
int decode_tag(cell_t cell) {
    int low2 = cell & 0x3;

    if (low2 == 3) {
        /* 11 - check bit 2 for LNT/EXT */
        return (cell & 0x4) ? TAG_EXT : TAG_LNT;
    } else if (low2 == 2) {
        /* 10 - check bit 2 for LST/LNT */
        return (cell & 0x4) ? TAG_LNT : TAG_LST;
    }

    return low2;  /* 00=XT, 01=LIT */
}

/* Decode LIT - extract 62-bit signed value */
int64_t decode_lit(cell_t cell) {
    /* Arithmetic right shift to preserve sign */
    return (int64_t)cell >> 2;
}

/* Decode XT - extract address */
void* decode_xt(cell_t cell) {
    return (void*)(cell & ~0x3ULL);
}

/* Decode LST - extract symbol ID (3-bit tag) */
uint64_t decode_lst(cell_t cell) {
    return cell >> 3;  /* Shift past 3-bit tag */
}

/* Decode LNT - extract count */
uint64_t decode_lnt(cell_t cell) {
    return cell >> 3;
}

/* Check if cell is EXIT */
bool is_exit(cell_t cell) {
    return cell == 0ULL;
}

/* Check if cell is LIT */
bool is_lit(cell_t cell) {
    return (cell & 0x3) == TAG_LIT;
}

/* Check if cell is XT */
bool is_xt(cell_t cell) {
    return (cell & 0x3) == TAG_XT;
}

/* Check if cell is LST */
bool is_lst(cell_t cell) {
    return (cell & 0x7) == TAG_LST;  /* Need 3 bits for LST */
}

/* Check if cell is LNT */
bool is_lnt(cell_t cell) {
    return (cell & 0x7) == TAG_LNT;  /* 110 */
}

/* Cell buffer management */

cell_buffer_t* cell_buffer_create(void) {
    cell_buffer_t* buf = malloc(sizeof(cell_buffer_t));
    if (!buf) return NULL;

    buf->capacity = 256;
    buf->count = 0;
    buf->cells = malloc(buf->capacity * sizeof(cell_t));

    if (!buf->cells) {
        free(buf);
        return NULL;
    }

    return buf;
}

void cell_buffer_free(cell_buffer_t* buf) {
    if (buf) {
        free(buf->cells);
        free(buf);
    }
}

void cell_buffer_append(cell_buffer_t* buf, cell_t cell) {
    if (buf->count >= buf->capacity) {
        buf->capacity *= 2;
        buf->cells = realloc(buf->cells, buf->capacity * sizeof(cell_t));
    }
    buf->cells[buf->count++] = cell;
}

void cell_buffer_clear(cell_buffer_t* buf) {
    buf->count = 0;
}
