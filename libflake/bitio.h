/**
 * Bitwise File Writer
 * Copyright (c) 2006-2007 Justin Ruggles
 *
 * derived from ffmpeg/libavcodec/bitstream.h
 * Common bit i/o utils
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2002-2004 Michael Niedermayer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef BITIO_H
#define BITIO_H

#include "common.h"

#include <assert.h>
#include "bswap.h"

typedef struct BitWriter {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buffer, *buf_ptr, *buf_end;
    int eof;
} BitWriter;

static inline void
bitwriter_init(BitWriter *bw, void *buf, int len)
{
    if(len < 0) {
        len = 0;
        buf = NULL;
    }
    bw->buffer = buf;
    bw->buf_end = bw->buffer + len;
    bw->buf_ptr = bw->buffer;
    bw->bit_left = 32;
    bw->bit_buf = 0;
    bw->eof = 0;
}

static inline uint32_t
bitwriter_count(BitWriter *bw)
{
    // TODO: simplify
    return ((((bw->buf_ptr - bw->buffer) << 3) + 32 - bw->bit_left) + 7) >> 3;
}

static inline void
bitwriter_flush(BitWriter *bw)
{
    bw->bit_buf <<= bw->bit_left;
    while(bw->bit_left < 32 && !bw->eof) {
        if(bw->buf_ptr >= bw->buf_end) {
            bw->eof = 1;
            break;
        }
        if(bw->buffer != NULL) {
            *bw->buf_ptr = bw->bit_buf >> 24;
        }
        bw->buf_ptr++;
        bw->bit_buf <<= 8;
        bw->bit_left += 8;
    }
    bw->bit_left = 32;
    bw->bit_buf = 0;
}

static inline void
bitwriter_writebits(BitWriter *bw, int bits, uint32_t val)
{
    uint32_t bb=0;
    assert(bits == 32 || val < (1U << bits));

    if(bits == 0 || bw->eof) return;
    if((bw->buf_ptr+3) >= bw->buf_end) {
        bw->eof = 1;
        return;
    }
    if(bits < bw->bit_left) {
        bw->bit_buf = (bw->bit_buf << bits) | val;
        bw->bit_left -= bits;
    } else {
        if(bw->bit_left == 32) {
            assert(bits == 32);
            bb = val;
        } else {
            bb = (bw->bit_buf << bw->bit_left) | (val >> (bits - bw->bit_left));
            bw->bit_left += (32 - bits);
        }
        if(bw->buffer != NULL) {
            *(uint32_t *)bw->buf_ptr = be2me_32(bb);
        }
        bw->buf_ptr += 4;
        bw->bit_buf = val;
    }
}

static inline void
bitwriter_writebits_signed(BitWriter *bw, int bits, int32_t val)
{
    assert(bits >= 0 && bits <= 31);
    bitwriter_writebits(bw, bits, val & ((1ULL<<bits)-1));
}

static inline void
bitwriter_write_rice_signed(BitWriter *bw, int k, int32_t val)
{
    int v, q;

    if(k < 0) return;

    // convert signed to unsigned
    v = -2*val-1;
    v ^= (v>>31);

    // write quotient in unary
    q = (v >> k) + 1;
    while(q > 31) {
        bitwriter_writebits(bw, 31, 0);
        q -= 31;
    }
    bitwriter_writebits(bw, q, 1);

    // write write remainder in binary using 'k' bits
    bitwriter_writebits(bw, k, v&((1<<k)-1));
}

#endif /* BITIO_H */
