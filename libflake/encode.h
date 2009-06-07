/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006-2007 Justin Ruggles
 *
 * Flake is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Flake is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Flake; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FLAC_H
#define FLAC_H

#include "config.h"

#include <inttypes.h>
#include "flake.h"
#include "rice.h"
#include "lpc.h"
#include "md5.h"

#define FLAKE_VERSION "SVN"

#define FLAC_MAX_CH  8
#define FLAC_MIN_BLOCKSIZE  16
#define FLAC_MAX_BLOCKSIZE  65535

#define FLAC_SUBFRAME_CONSTANT  0
#define FLAC_SUBFRAME_VERBATIM  1
#define FLAC_SUBFRAME_FIXED     8
#define FLAC_SUBFRAME_LPC      32

#define FLAC_CHMODE_NOT_STEREO      0
#define FLAC_CHMODE_LEFT_RIGHT      1
#define FLAC_CHMODE_LEFT_SIDE       8
#define FLAC_CHMODE_RIGHT_SIDE      9
#define FLAC_CHMODE_MID_SIDE       10

#define FLAC_STREAM_MARKER  0x664C6143

struct BitWriter;

typedef struct FlacSubframe {
    int type;
    int type_code;
    int wasted_bits;
    int order;
    int obits;
    int32_t coefs[MAX_LPC_ORDER];
    int shift;
    int32_t samples[FLAC_MAX_BLOCKSIZE];
    int32_t residual[FLAC_MAX_BLOCKSIZE];
    RiceContext rc;
} FlacSubframe;

typedef struct FlacFrame {
    int blocksize;
    int verbatim_size;
    int bs_code[2];
    int ch_mode;
    uint8_t crc8;
    FlacSubframe subframes[FLAC_MAX_CH];
} FlacFrame;

typedef struct FlacEncodeContext {
    int channels;
    int ch_code;
    int samplerate;
    int sr_code[2];
    int bps;
    int bps_code;
    uint32_t sample_count;
    FlakeEncodeParams params;
    int max_frame_size;
    int lpc_precision;
    uint32_t frame_count;
    FlacFrame frame;
    MD5Context md5ctx;
    struct BitWriter *bw;
    uint8_t *frame_buffer;
    int frame_buffer_size;
    int last_frame;
    FlakeContext *parent;
} FlacEncodeContext;

extern int encode_frame(FlacEncodeContext *s, uint8_t *frame_buffer,
                        int buf_size, const int32_t *samples, int block_size);

#endif /* FLAC_H */
