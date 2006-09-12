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

#ifndef FLAKE_H
#define FLAKE_H

#define FLAKE_VERSION SVN
#define FLAKE_IDENT   "Flake SVN"

typedef struct FlakeContext {

    // number of audio channels
    // set by user prior to calling flake_encode_init
    // valid values are 1 to 8
    int channels;

    // audio sample rate in Hz
    // set by user prior to calling flake_encode_init
    int sample_rate;

    // sample size in bits
    // set by user prior to calling flake_encode_init
    // only 16-bit is currently supported
    int bits_per_sample;

    // total stream samples
    // set by user prior to calling flake_encode_init
    // if 0, stream length is unknown
    unsigned int samples;

    // compression quality
    // set by user prior to calling flake_encode_init
    // standard values are 0 to 8
    // 0 is lower compression, faster encoding
    // 8 is higher compression, slower encoding
    // extended values 9 to 12 are slower and/or use
    // higher prediction orders
    int compression;

    // prediction order selection method
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 5
    // 0 = use maximum order only
    // 1 = use estimation
    // 2 = 2-level
    // 3 = 4-level
    // 4 = 8-level
    // 5 = full search
    int order_method;

    // stereo decorrelation method
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 2
    // 0 = independent L+R channels
    // 1 = mid-side encoding
    int stereo_method;

    // block size in samples
    // set by the user prior to calling flake_encode_init
    // if set to 0, a block size is chosen automatically
    // can also be changed by user before encoding a frame
    int block_size;

    // padding size in bytes
    // set by the user prior to calling flake_encode_init
    // if set to less than 0, defaults to 4096
    int padding_size;

    // maximum encoded frame size
    // this is set by flake_encode_init based on input audio format
    // it can be used by the user to allocate an output buffer
    int max_frame_size;

    // maximum prediction order
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 32
    int max_order;

    // MD5 digest
    // set by flake_encode_close;
    unsigned char md5digest[16];

    // header bytes
    // allocated by flake_encode_init and freed by flake_encode_close
    unsigned char *header;

    // encoding context, which is hidden from the user
    // allocated by flake_encode_init and freed by flake_encode_close
    void *private_ctx;

} FlakeContext;

extern int flake_encode_init(FlakeContext *s);

extern int flake_encode_frame(FlakeContext *s, unsigned char *frame_buffer,
                              short *samples);

extern void flake_encode_close(FlakeContext *s);

#endif /* FLAKE_H */
