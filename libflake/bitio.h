/**
 * Bitwise File Writer
 * Copyright (c) 2006 Justin Ruggles
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

#include <inttypes.h>

typedef struct BitWriter {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buffer, *buf_ptr, *buf_end;
    int eof;
} BitWriter;

extern void bitwriter_init(BitWriter *bw, void *buf, int len);

extern uint32_t bitwriter_count(BitWriter *bw);

extern void bitwriter_flush(BitWriter *bw);

extern void bitwriter_writebits(BitWriter *bw, int bits, uint32_t val);

extern void bitwriter_writebits_signed(BitWriter *bw, int bits, int32_t val);

extern void bitwriter_write_rice_signed(BitWriter *bw, int k, int32_t val);

#endif /* BITIO_H */
