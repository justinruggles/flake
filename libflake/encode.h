/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006 Justin Ruggles
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

#ifndef FLAC_H
#define FLAC_H

#include "config.h"

#include <inttypes.h>
#include "bitio.h"
#include "rice.h"
#include "lpc.h"
#include "md5.h"

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

typedef struct FlacSubframe {
    int type;
    int type_code;
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
    int bs_code[2];
    int ch_mode;
    int ch_order[2];
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
    int blocksize;
    int block_time_ms;
    int max_predictor_order;
    int min_partition_order;
    int max_partition_order;
    int lpc_precision;
    int max_framesize;
    int order_method;
    int stereo_method;
    int padding_size;
    uint32_t frame_count;
    FlacFrame frame;
    MD5Context md5ctx;
    BitWriter bw;
} FlacEncodeContext;

static const int flac_samplerates[16] = {
    0, 0, 0, 0,
    8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000,
    0, 0, 0, 0
};

static const int flac_bitdepths[8] = {
    0, 8, 12, 0, 16, 20, 24, 0
};

static const int flac_blocksizes[15] = {
    0,
    192,
    576, 1152, 2304, 4608,
    0, 0,
    256, 512, 1024, 2048, 4096, 8192, 16384
};

static const int flac_blocksizes_ordered[13] = {
    0, 192, 256, 512, 576, 1024, 1152, 2048, 2304, 4096, 4608, 8192, 16384
};

#endif /* FLAC_H */
