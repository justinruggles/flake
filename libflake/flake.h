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

/* shared library API export */
#if defined(_WIN32) && !defined(_XBOX)
 #if defined(FLAKE_BUILD_LIBRARY)
  #define FLAKE_API __declspec(dllexport)
 #else
  #define FLAKE_API __declspec(dllimport)
 #endif
#else
 #if defined(FLAKE_BUILD_LIBRARY) && defined(HAVE_GCC_VISIBILITY)
  #define FLAKE_API __attribute__((visibility("default")))
 #else
  #define FLAKE_API extern
 #endif
#endif

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
    // Values 0 to 8 are libFLAC compatibility modes
    // 0 is lower compression, faster encoding
    // 8 is higher compression, slower encoding
    // extended values 9 to 12 are specific to Flake
    // 9 and 10 are within the FLAC Subset and use variable block size
    // 11 and 12 use parameters such as higher block sizes and prediction
    // orders which are outside of the FLAC Subset, so should not be used for
    // streaming
    int compression;

    // prediction order selection method
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
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 2
    // 0 = independent L+R channels
    // 1 = mid-side encoding
    int stereo_method;

    // block size in samples
    // if set to 0, a block size is chosen based on block_time_ms
    int block_size;

    // padding size in bytes
    // if set to less than 0, defaults to 8192
    int padding_size;

    // minimum prediction order
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 4 for fixed prediction and 1 to 32 for non-fixed
    int min_prediction_order;

    // maximum prediction order
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 4 for fixed prediction and 1 to 32 for non-fixed
    int max_prediction_order;

    // type of linear prediction
    // if set to less than 0, it is chosen based on compression.
    // 0 = fixed prediction
    // 1 = Levinson-Durbin recursion
    int prediction_type;

    // minimum partition order
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 8
    int min_partition_order;

    // maximum partition order
    // if set to less than 0, it is chosen based on compression.
    // valid values are 0 to 8
    int max_partition_order;

    // whether to use variable block sizes
    // if set to 1, libflake will automatically split each frame into smaller
    // frames in order to improve compression
    // 0 = fixed block size
    // 1 = variable block size
    int variable_block_size;

    // whether to let the user send frames with varying block size to the
    // encoder this will allow a user to experiment with their own variable
    // block size algorithms if they choose to do so.
    int allow_vbs;

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

    // encoding parameters
    // all fields must be set by user (or by flake_set_defaults) prior to
    // calling flake_encode_init
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
FLAKE_API int flake_set_defaults(FlakeEncodeParams *params);

/**
 * Validates encoding parameters
 * @return -1 if error. 0 if ok. 1 if ok but non-Subset.
 */
FLAKE_API int flake_validate_params(const FlakeContext *s);

FLAKE_API int flake_encode_init(FlakeContext *s);

FLAKE_API void *flake_get_buffer(const FlakeContext *s);

FLAKE_API int flake_encode_frame(FlakeContext *s, const short *samples,
                                 int block_size);

FLAKE_API void flake_encode_close(FlakeContext *s);

FLAKE_API const char *flake_get_version(void);

/**
 * FLAC Streaminfo Metadata
 */
typedef struct FlakeStreaminfo {
    unsigned int min_block_size;
    unsigned int max_block_size;
    unsigned int min_frame_size;
    unsigned int max_frame_size;
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int bits_per_sample;
    unsigned int samples;
    unsigned char md5sum[16];
} FlakeStreaminfo;

FLAKE_API int flake_metadata_get_streaminfo(const FlakeContext *s,
                                            FlakeStreaminfo *strminfo);

FLAKE_API void flake_metadata_write_streaminfo(const FlakeStreaminfo *strminfo,
                                               unsigned char *data);

typedef struct FlakeVorbisComment {
    char *vendor_string;
    unsigned int num_entries;
    char *entries[1024];
} FlakeVorbisComment;

FLAKE_API void flake_metadata_init_vorbiscomment(FlakeVorbisComment *vc);

FLAKE_API int flake_metadata_add_vorbiscomment_entry(FlakeVorbisComment *vc,
                                                     char *entry);

FLAKE_API int flake_metadata_get_vorbiscomment_size(const FlakeVorbisComment *vc);

FLAKE_API int flake_metadata_write_vorbiscomment(const FlakeVorbisComment *vc,
                                                 unsigned char *data);

#endif /* FLAKE_H */
