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

#ifndef FLAKE_H
#define FLAKE_H

#define FLAKE_VERSION SVN
#define FLAKE_IDENT   "Flake SVN"

typedef enum {
    FLAKE_ORDER_METHOD_MAX,
    FLAKE_ORDER_METHOD_EST,
    FLAKE_ORDER_METHOD_2LEVEL,
    FLAKE_ORDER_METHOD_4LEVEL,
    FLAKE_ORDER_METHOD_8LEVEL,
    FLAKE_ORDER_METHOD_SEARCH,
    FLAKE_ORDER_METHOD_LOG
} FlakeOrderMethod;

typedef enum {
    FLAKE_STEREO_METHOD_INDEPENDENT,
    FLAKE_STEREO_METHOD_ESTIMATE
} FlakeStereoMethod;

typedef enum {
    FLAKE_PREDICTION_NONE,
    FLAKE_PREDICTION_FIXED,
    FLAKE_PREDICTION_LEVINSON,
} FlakePrediction;

typedef struct FlakeEncodeParams {

    // compression quality
    // set by user prior to calling flake_encode_init
    // standard values are 0 to 8
    // 0 is lower compression, faster encoding
    // 8 is higher compression, slower encoding
    // extended values 9 to 13 are slower and/or use
    // higher prediction orders
    int compression;

    // prediction order selection method
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 6
    // 0 = use maximum order only
    // 1 = use estimation
    // 2 = 2-level
    // 3 = 4-level
    // 4 = 8-level
    // 5 = full search
    // 6 = log search
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
    // if set to 0, a block size is chosen based on block_time_ms
    int block_size;

    // padding size in bytes
    // set by the user prior to calling flake_encode_init
    // if set to less than 0, defaults to 4096
    int padding_size;

    // minimum prediction order
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 4 for fixed prediction and 1 to 32 for non-fixed
    int min_prediction_order;

    // maximum prediction order
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 4 for fixed prediction and 1 to 32 for non-fixed
    int max_prediction_order;

    // type of linear prediction
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // 0 = fixed prediction
    // 1 = Levinson-Durbin recursion
    int prediction_type;

    // minimum partition order
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 8
    int min_partition_order;

    // maximum partition order
    // set by user prior to calling flake_encode_init
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 8
    int max_partition_order;

    // whether to use variable block sizes
    // set by user prior to calling flake_encode_init
    // 0 = fixed block size
    // 1 = variable block size
    int variable_block_size;

} FlakeEncodeParams;

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

    FlakeEncodeParams params;

    // header bytes
    // allocated by flake_encode_init and freed by flake_encode_close
    unsigned char *header;

    // encoding context, which is hidden from the user
    // allocated by flake_encode_init and freed by flake_encode_close
    void *private_ctx;

} FlakeContext;

/**
 * Sets encoding defaults based on compression level
 * params->compression must be set prior to calling
 */
extern int flake_set_defaults(FlakeEncodeParams *params);

/**
 * Validates encoding parameters
 * @return -1 if error. 0 if ok. 1 if ok but non-Subset.
 */
extern int flake_validate_params(FlakeContext *s);

extern int flake_encode_init(FlakeContext *s);

extern void *flake_get_buffer(FlakeContext *s);

extern int flake_encode_frame(FlakeContext *s, short *samples, int block_size);

extern void flake_encode_close(FlakeContext *s);

extern int flake_get_max_frame_size(FlakeContext *s);

extern void flake_get_md5sum(FlakeContext *s, unsigned char *md5sum);

#endif /* FLAKE_H */
