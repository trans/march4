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
        /* 11 - Reserved (currently unused) */
        return 0;  /* Invalid tag */
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

/* ============================================================================ */
/* Blob buffer management (for CID-based storage) */
/* ============================================================================ */

blob_buffer_t* blob_buffer_create(void) {
    blob_buffer_t* buf = malloc(sizeof(blob_buffer_t));
    if (!buf) return NULL;

    buf->capacity = 256;
    buf->size = 0;
    buf->data = malloc(buf->capacity);

    if (!buf->data) {
        free(buf);
        return NULL;
    }

    return buf;
}

void blob_buffer_free(blob_buffer_t* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

void blob_buffer_clear(blob_buffer_t* buf) {
    buf->size = 0;
}

void blob_buffer_append_u16(blob_buffer_t* buf, uint16_t value) {
    /* Ensure capacity */
    if (buf->size + 2 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = realloc(buf->data, buf->capacity);
    }

    /* Write as little-endian */
    buf->data[buf->size++] = value & 0xFF;
    buf->data[buf->size++] = (value >> 8) & 0xFF;
}

void blob_buffer_append_bytes(blob_buffer_t* buf, const uint8_t* bytes, size_t len) {
    /* Ensure capacity */
    while (buf->size + len > buf->capacity) {
        buf->capacity *= 2;
        buf->data = realloc(buf->data, buf->capacity);
    }

    /* Copy bytes */
    memcpy(buf->data + buf->size, bytes, len);
    buf->size += len;
}

/* ============================================================================ */
/* CID-based encoding functions (LINKING.md design) */
/* ============================================================================ */

/* Encode primitive reference - 2-byte tag only
 * Tag format: (prim_id << 1) | 0
 * Bit 0 = 0 means primitive (no CID follows)
 */
void encode_primitive(blob_buffer_t* buf, uint16_t prim_id) {
    uint16_t tag = (prim_id << 1) | 0;  /* Bit 0 = 0 */
    blob_buffer_append_u16(buf, tag);
}

/* Encode CID reference - 2-byte tag + 32-byte binary CID
 * Tag format: (kind << 1) | 1
 * Bit 0 = 1 means CID follows
 */
void encode_cid_ref(blob_buffer_t* buf, uint16_t kind, const unsigned char* cid) {
    uint16_t tag = (kind << 1) | 1;  /* Bit 0 = 1 */
    blob_buffer_append_u16(buf, tag);
    blob_buffer_append_bytes(buf, cid, CID_SIZE);
}

/* Encode inline i64 literal - 2-byte tag + 8-byte value
 * Tag format: (PRIM_LIT << 1) | 0 = 0x0000
 * Bit 0 = 0 (primitive type), ID = 0 (special: literal)
 */
void encode_inline_literal(blob_buffer_t* buf, int64_t value) {
    uint16_t tag = (PRIM_LIT << 1) | 0;  /* Tag = 0x0000 */
    blob_buffer_append_u16(buf, tag);

    /* Append value as little-endian 8 bytes */
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (value >> (i * 8)) & 0xFF;
    }
    blob_buffer_append_bytes(buf, bytes, 8);
}

/* ============================================================================ */
/* CID-based decoding functions (LINKING.md design) */
/* ============================================================================ */

/* Decode tag from blob data
 * Returns: pointer to next position in blob
 * Outputs:
 *   is_cid: true if CID reference, false if primitive/literal
 *   id_or_kind: primitive ID or blob kind
 *   cid: pointer to CID (if is_cid=true), literal bytes (if PRIM_LIT), or NULL
 */
const uint8_t* decode_tag_ex(const uint8_t* ptr, bool* is_cid, uint16_t* id_or_kind, const unsigned char** cid) {
    /* Read 2-byte tag (little-endian) */
    uint16_t tag = ptr[0] | (ptr[1] << 8);
    ptr += 2;

    /* Check bit 0 */
    if (tag & 1) {
        /* CID reference */
        *is_cid = true;
        *id_or_kind = tag >> 1;  /* Blob kind */
        *cid = ptr;
        return ptr + CID_SIZE;  /* Skip CID */
    } else {
        /* Primitive or literal */
        *is_cid = false;
        *id_or_kind = tag >> 1;  /* Primitive ID */

        if (*id_or_kind == PRIM_LIT) {
            /* Literal: point cid to 8-byte value */
            *cid = ptr;
            return ptr + 8;  /* Skip 8-byte literal */
        } else {
            /* Regular primitive: no data follows */
            *cid = NULL;
            return ptr;
        }
    }
}
