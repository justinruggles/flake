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

#include "common.h"

#include "encode.h"
#include "flake.h"
#include "bitio.h"
#include "crc.h"
#include "lpc.h"
#include "md5.h"
#include "optimize.h"
#include "rice.h"
#include "vbs.h"


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


static void
write_metadata_header(int last, int type, int size, uint8_t *buf)
{
    BitWriter bw;
    bitwriter_init(&bw, buf, 4);
    bitwriter_writebits(&bw,  1, last);
    bitwriter_writebits(&bw,  7, type);
    bitwriter_writebits(&bw, 24, size);
    bitwriter_flush(&bw);
}

/**
 * Write streaminfo metadata block to byte array
 */
static void
write_streaminfo(FlacEncodeContext *ctx, uint8_t *streaminfo, int last)
{
    FlakeStreaminfo strminfo;

    write_metadata_header(last, 0, 34, streaminfo);

    if(flake_get_streaminfo(ctx->parent, &strminfo))
        return;
    flake_write_streaminfo(&strminfo, &streaminfo[4]);
}

/**
 * Write padding metadata block to byte array.
 */
static int
write_padding(uint8_t *padding, int last, int padlen)
{
    write_metadata_header(last, 1, padlen, padding);
    return padlen + 4;
}

/**
 * Write vorbis comment metadata block to byte array.
 * Just writes the vendor string for now.
 */
static int
write_vorbis_comment(uint8_t *comment, int last)
{
    int vc_size;
    FlakeVorbisComment vc;

    // init and get size
    flake_init_vorbiscomment(&vc);
    vc_size = flake_get_vorbiscomment_size(&vc);
    if(vc_size < 0)
        vc_size = 8;

    // metadata header
    write_metadata_header(last, 4, vc_size, comment);

    // write entry
    if(vc_size > 8) {
        if(flake_write_vorbiscomment(&vc, &comment[4])) {
            vc_size = 8;
            write_metadata_header(last, 4, vc_size, comment);
            memset(&comment[4], 0, 8);
        }
    } else {
        memset(&comment[4], 0, 8);
    }

    // return total metadata block size
    return 4 + vc_size;
}

/**
 * Write fLaC stream marker & metadata headers
 */
static int
write_headers(FlacEncodeContext *ctx, uint8_t *header)
{
    int header_size, last;

    header_size = 0;
    last = 0;

    // stream marker
    header[0] = 0x66;
    header[1] = 0x4C;
    header[2] = 0x61;
    header[3] = 0x43;
    header_size += 4;

    // streaminfo
    write_streaminfo(ctx, &header[header_size], last);
    header_size += 38;

    // vorbis comment
    if(ctx->params.padding_size == 0) last = 1;
    header_size += write_vorbis_comment(&header[header_size], last);

    // padding
    if(ctx->params.padding_size > 0) {
        last = 1;
        header_size += write_padding(&header[header_size], last,
                                     ctx->params.padding_size);
    }

    return header_size;
}

int
flake_set_defaults(FlakeEncodeParams *params)
{
    int lvl;

    if(!params) {
        return -1;
    }
    lvl = params->compression;
    if(lvl < 0 || lvl > 12) {
        return -1;
    }

    // default to level 5 params
    params->order_method = FLAKE_ORDER_METHOD_EST;
    params->stereo_method = FLAKE_STEREO_METHOD_ESTIMATE;
    params->block_size =  4096;
    params->prediction_type = FLAKE_PREDICTION_LEVINSON;
    params->min_prediction_order = 1;
    params->max_prediction_order = 8;
    params->min_partition_order = 0;
    params->max_partition_order = 5;
    params->padding_size = 8192;
    params->variable_block_size = 0;
    params->allow_vbs = 0;

    // differences from level 5
    switch(lvl) {
        case 0:
            params->stereo_method = FLAKE_STEREO_METHOD_INDEPENDENT;
            params->block_size = 1152;
            params->prediction_type = FLAKE_PREDICTION_FIXED;
            params->min_prediction_order = 2;
            params->max_prediction_order = 2;
            params->min_partition_order = 0;
            params->max_partition_order = 3;
            break;
        case 1:
            params->block_size = 1152;
            params->prediction_type = FLAKE_PREDICTION_FIXED;
            params->min_prediction_order = 2;
            params->max_prediction_order = 4;
            params->min_partition_order = 0;
            params->max_partition_order = 3;
            break;
        case 2:
            params->block_size = 1152;
            params->prediction_type = FLAKE_PREDICTION_FIXED;
            params->min_prediction_order = 0;
            params->max_prediction_order = 4;
            params->min_partition_order = 0;
            params->max_partition_order = 3;
            break;
        case 3:
            params->stereo_method = FLAKE_STEREO_METHOD_INDEPENDENT;
            params->max_prediction_order = 6;
            params->max_partition_order = 4;
            break;
        case 4:
            params->max_partition_order = 4;
            break;
        case 5:
            break;
        case 6:
            params->max_partition_order = 6;
            break;
        case 7:
            params->order_method = FLAKE_ORDER_METHOD_4LEVEL;
            params->max_partition_order = 6;
            break;
        case 8:
            params->order_method = FLAKE_ORDER_METHOD_LOG;
            params->max_prediction_order = 12;
            params->max_partition_order = 6;
            break;
        case 9:
            params->order_method = FLAKE_ORDER_METHOD_LOG;
            params->max_prediction_order = 12;
            params->max_partition_order = 8;
            params->variable_block_size = 1;
            break;
        case 10:
            params->order_method = FLAKE_ORDER_METHOD_SEARCH;
            params->max_prediction_order = 12;
            params->max_partition_order = 8;
            params->variable_block_size = 1;
            break;
        case 11:
            params->block_size = 8192;
            params->order_method = FLAKE_ORDER_METHOD_LOG;
            params->max_prediction_order = 32;
            params->max_partition_order = 8;
            params->variable_block_size = 1;
            break;
        case 12:
            params->block_size = 8192;
            params->order_method = FLAKE_ORDER_METHOD_SEARCH;
            params->max_prediction_order = 32;
            params->max_partition_order = 8;
            params->variable_block_size = 1;
            break;
    }

    return 0;
}

int
flake_validate_params(const FlakeContext *s)
{
    int subset = 0;
    int bs;
    const FlakeEncodeParams *params;

    if(s == NULL) {
        return -1;
    }
    params = &s->params;

    if(s->channels < 1 || s->channels > FLAC_MAX_CH) {
        return -1;
    }

    if(s->sample_rate < 1 || s->sample_rate > 655350) {
        return -1;
    }

    if(s->bits_per_sample < 4 || s->bits_per_sample > 32) {
        return -1;
    }
    if(s->bits_per_sample < 8 || s->bits_per_sample > 24 ||
            s->bits_per_sample % 4 != 0) {
       subset = 1;
    }

    if(params->compression < 0 || params->compression > 12) {
        return -1;
    }

    if(params->order_method < 0 || params->order_method > 6) {
        return -1;
    }

    if(params->stereo_method < 0 || params->stereo_method > 1) {
        return -1;
    }

    bs = params->block_size;
    if(bs < FLAC_MIN_BLOCKSIZE || bs > FLAC_MAX_BLOCKSIZE) {
        return -1;
    }
    if(s->sample_rate <= 48000 && bs > 4608) {
        subset = 1;
    }

    if(params->prediction_type < 0 || params->prediction_type > 2) {
        return -1;
    }

    if(params->min_prediction_order > params->max_prediction_order) {
        return -1;
    }
    if(params->prediction_type == FLAKE_PREDICTION_FIXED) {
        if(params->min_prediction_order < 0 ||
           params->min_prediction_order > 4) {
            return -1;
        }
        if(params->max_prediction_order < 0 ||
           params->max_prediction_order > 4) {
            return -1;
        }
    } else {
        if(params->min_prediction_order < 1 ||
           params->min_prediction_order > 32) {
            return -1;
        }
        if(params->max_prediction_order < 1 ||
           params->max_prediction_order > 32) {
            return -1;
        }
        if(s->sample_rate <= 48000 && params->max_prediction_order > 12) {
            subset = 1;
        }
    }

    if(params->min_partition_order > params->max_partition_order) {
        return -1;
    }
    if(params->min_partition_order < 0 || params->min_partition_order > 8) {
        return -1;
    }
    if(params->max_partition_order < 0 || params->max_partition_order > 8) {
        return -1;
    }

    if(params->padding_size < 0 || params->padding_size >= (1<<24)) {
        return -1;
    }

    if(params->variable_block_size < 0 || params->variable_block_size > 1) {
        return -1;
    }

    // don't allow block size of 16 in variable block size mode. this is a bug
    // in the spec which has been fixed in FLAC 1.2.0, but is not backwards
    // compatible.  this constraint will be removed when Flake is updated to
    // the new spec version.
    if(bs == 16 && (params->variable_block_size > 0 || params->allow_vbs)) {
        return -1;
    }

    return subset;
}

/**
 * Initialize encoder
 */
int
flake_encode_init(FlakeContext *s)
{
    FlacEncodeContext *ctx;
    int i, header_len;

    if(s == NULL) {
        return -1;
    }

    // allocate memory
    ctx = calloc(1, sizeof(FlacEncodeContext));
    s->private_ctx = ctx;
    ctx->parent = s;

    if(flake_validate_params(s) < 0) {
        return -1;
    }

    ctx->channels = s->channels;
    ctx->ch_code = s->channels-1;

    // find samplerate in table
    for(i=4; i<12; i++) {
        if(s->sample_rate == flac_samplerates[i]) {
            ctx->samplerate = flac_samplerates[i];
            ctx->sr_code[0] = i;
            ctx->sr_code[1] = 0;
            break;
        }
    }
    // if not in table, samplerate is non-standard
    if(i == 12) {
        ctx->samplerate = s->sample_rate;
        if(ctx->samplerate % 1000 == 0 && ctx->samplerate <= 255000) {
            ctx->sr_code[0] = 12;
            ctx->sr_code[1] = ctx->samplerate / 1000;
        } else if(ctx->samplerate % 10 == 0 && ctx->samplerate <= 655350) {
            ctx->sr_code[0] = 14;
            ctx->sr_code[1] = s->sample_rate / 10;
        } else if(ctx->samplerate < 65535) {
            ctx->sr_code[0] = 13;
            ctx->sr_code[1] = ctx->samplerate;
        }
    }

    for(i=1; i<8; i++) {
        if(s->bits_per_sample == flac_bitdepths[i]) {
            ctx->bps = flac_bitdepths[i];
            ctx->bps_code = i;
            break;
        }
    }
    if(i == 8) {
        ctx->bps = s->bits_per_sample;
        ctx->bps_code = 0;
    }


    ctx->sample_count = s->samples;

    ctx->params = s->params;

    // right now 15-bit precision seems to generally work better than adaptive.
    // TODO: try adapting based on prediction order, not blocksize
    ctx->lpc_precision = 15;

    // set maximum encoded frame size (if larger, re-encodes in verbatim mode)
    if(ctx->channels == 2) {
        ctx->max_frame_size = 16 + ((ctx->params.block_size * (ctx->bps+ctx->bps+1) + 7) >> 3);
    } else {
        ctx->max_frame_size = 16 + ((ctx->params.block_size * ctx->channels * ctx->bps + 7) >> 3);
    }

    // initialize frame buffer
    ctx->frame_buffer_size = ctx->max_frame_size * 3 / 2;
    ctx->frame_buffer = calloc(ctx->frame_buffer_size, 1);

    // output header bytes
    ctx->bw = calloc(sizeof(BitWriter), 1);
    s->header = calloc(ctx->params.padding_size + 1024, 1);
    header_len = -1;
    if(s->header != NULL) {
        header_len = write_headers(ctx, s->header);
    }

    ctx->frame_count = 0;
    ctx->last_frame = 0;

    // initialize CRC & MD5
    crc_init();
    md5_init(&ctx->md5ctx);

    return header_len;
}

void *
flake_get_buffer(const FlakeContext *s)
{
    FlacEncodeContext *ctx;

    if(!s)
        return NULL;
    ctx = (FlacEncodeContext *) s->private_ctx;
    if(!ctx)
        return NULL;
    return ctx->frame_buffer;
}

/**
 * Initialize the current frame before encoding
 */
static int
init_frame(FlacEncodeContext *ctx, int block_size)
{
    int i, ch;
    FlacFrame *frame;

    frame = &ctx->frame;

    if(block_size < 1 || block_size > FLAC_MAX_BLOCKSIZE) {
        return -1;
    }

    // get block size codes
    for(i=0; i<15; i++) {
        if(block_size == flac_blocksizes[i]) {
            frame->blocksize = flac_blocksizes[i];
            frame->bs_code[0] = i;
            frame->bs_code[1] = -1;
            break;
        }
    }
    if(i == 15) {
        frame->blocksize = block_size;
        if(frame->blocksize <= 256) {
            frame->bs_code[0] = 6;
            frame->bs_code[1] = frame->blocksize-1;
        } else {
            frame->bs_code[0] = 7;
            frame->bs_code[1] = frame->blocksize-1;
        }
    }

    // set frame size cutoff. if larger, re-encodes in verbatim mode
    if(ctx->channels == 2) {
        frame->verbatim_size = 16 + ((frame->blocksize * (ctx->bps+ctx->bps+1) + 7) >> 3);
    } else {
        frame->verbatim_size = 16 + ((frame->blocksize * ctx->channels * ctx->bps + 7) >> 3);
    }

    // initialize output bps for each channel
    for(ch=0; ch<ctx->channels; ch++) {
        frame->subframes[ch].obits = ctx->bps;
        frame->subframes[ch].wasted_bits = 0;
    }

    return 0;
}

/**
 * Copy channel-interleaved input samples into separate subframes
 */
static void
copy_samples(FlacEncodeContext *ctx, const int32_t *samples)
{
    int i, j, ch;
    FlacFrame *frame;

    frame = &ctx->frame;
    for(i=0,j=0; i<frame->blocksize; i++) {
        for(ch=0; ch<ctx->channels; ch++,j++) {
            frame->subframes[ch].samples[i] = samples[j];
        }
    }
}

/**
 * Shift out any zero bits and set the wasted_bits parameter.
 */
static void
remove_wasted_bits(FlacEncodeContext *ctx)
{
    int i, ch, b;
    int wasted;
    FlacFrame *frame;
    int32_t *samples;

    frame = &ctx->frame;
    for (ch = 0; ch < ctx->channels; ch++) {
        wasted = ctx->bps-1;
        samples = frame->subframes[ch].samples;
        for (i = 0; i < frame->blocksize; i++) {
            int32_t s = samples[i];
            uint32_t mask = 0x1;
            for (b = 0; b <= wasted; b++) {
                if (s & mask)
                    break;
                mask <<= 1;
            }
            if (b < wasted)
                wasted = b;
            if (!wasted)
                break;
        }
        if (wasted == ctx->bps-1) {
            wasted = 0;
        } else if (wasted) {
            for (i = 0; i < frame->blocksize; i++) {
                samples[i] >>= wasted;
            }
            frame->subframes[ch].obits -= wasted;
        }
        frame->subframes[ch].wasted_bits = wasted;
    }
}

/**
 * Estimate the best stereo decorrelation mode
 */
static int
calc_decorr_scores(int32_t *left_ch, int32_t *right_ch, int n)
{
    int i, best;
    int32_t lt, rt;
    uint64_t sum[4];
    uint64_t score[4];
    int k;

    // calculate sum of 2nd order residual for each channel
    sum[0] = sum[1] = sum[2] = sum[3] = 0;
    for(i=2; i<n; i++) {
        lt = left_ch[i] - 2*left_ch[i-1] + left_ch[i-2];
        rt = right_ch[i] - 2*right_ch[i-1] + right_ch[i-2];
        sum[2] += abs((lt + rt) >> 1);
        sum[3] += abs(lt - rt);
        sum[0] += abs(lt);
        sum[1] += abs(rt);
    }
    // estimate bit counts
    for(i=0; i<4; i++) {
        k = find_optimal_rice_param(2*sum[i], n);
        sum[i] = rice_encode_count(2*sum[i], n, k);
    }

    // calculate score for each mode
    score[0] = sum[0] + sum[1];
    score[1] = sum[0] + sum[3];
    score[2] = sum[1] + sum[3];
    score[3] = sum[2] + sum[3];

    // return mode with lowest score
    best = 0;
    for(i=1; i<4; i++) {
        if(score[i] < score[best]) {
            best = i;
        }
    }
    switch(best) {
        case 0: return FLAC_CHMODE_LEFT_RIGHT;
        case 1: return FLAC_CHMODE_LEFT_SIDE;
        case 2: return FLAC_CHMODE_RIGHT_SIDE;
        case 3: return FLAC_CHMODE_MID_SIDE;
    }
    return FLAC_CHMODE_LEFT_RIGHT;
}

/**
 * Perform stereo channel decorrelation
 */
static void
channel_decorrelation(FlacEncodeContext *ctx)
{
    int i;
    FlacFrame *frame;
    int32_t *left, *right;
    int32_t tmp;

    frame = &ctx->frame;
    left  = frame->subframes[0].samples;
    right = frame->subframes[1].samples;

    if(ctx->channels != 2) {
        frame->ch_mode = FLAC_CHMODE_NOT_STEREO;
        return;
    }
    if(frame->blocksize <= 32 || ctx->params.stereo_method == FLAKE_STEREO_METHOD_INDEPENDENT) {
        frame->ch_mode = FLAC_CHMODE_LEFT_RIGHT;
        return;
    }

    // estimate stereo decorrelation type
    frame->ch_mode = calc_decorr_scores(left, right, frame->blocksize);

    // perform decorrelation and adjust bits-per-sample
    if(frame->ch_mode == FLAC_CHMODE_LEFT_RIGHT) {
        return;
    }
    if(frame->ch_mode == FLAC_CHMODE_MID_SIDE) {
        for(i=0; i<frame->blocksize; i++) {
            tmp = left[i];
            left[i] = (left[i] + right[i]) >> 1;
            right[i] = tmp - right[i];
        }
        frame->subframes[1].obits++;
    } else if(frame->ch_mode == FLAC_CHMODE_LEFT_SIDE) {
        for(i=0; i<frame->blocksize; i++) {
            right[i] = left[i] - right[i];
        }
        frame->subframes[1].obits++;
    } else if(frame->ch_mode == FLAC_CHMODE_RIGHT_SIDE) {
        for(i=0; i<frame->blocksize; i++) {
            left[i] = left[i] - right[i];
        }
        frame->subframes[0].obits++;
    }
}

/**
 * Write UTF-8 encoded integer value
 * Used to encode frame number in frame header
 */
static void
write_utf8(BitWriter *bw, uint32_t val)
{
    int bytes, shift;

    if(val < 0x80){
        bitwriter_writebits(bw, 8, val);
        return;
    }
    bytes = (log2i(val)+4) / 5;
    shift = (bytes - 1) * 6;
    bitwriter_writebits(bw, 8, (256 - (256>>bytes)) | (val >> shift));
    while(shift >= 6){
        shift -= 6;
        bitwriter_writebits(bw, 8, 0x80 | ((val >> shift) & 0x3F));
    }
}

static void
output_frame_header(FlacEncodeContext *ctx)
{
    FlacFrame *frame;
    uint8_t crc;

    frame = &ctx->frame;

    bitwriter_writebits(ctx->bw, 15, 0x7FFC);

    // new bitstream syntax for variable blocksize in FLAC 1.2.0
    bitwriter_writebits(ctx->bw, 1, ctx->params.variable_block_size > 0);

    bitwriter_writebits(ctx->bw, 4, frame->bs_code[0]);
    bitwriter_writebits(ctx->bw, 4, ctx->sr_code[0]);
    if(frame->ch_mode == FLAC_CHMODE_NOT_STEREO) {
        bitwriter_writebits(ctx->bw, 4, ctx->ch_code);
    } else {
        bitwriter_writebits(ctx->bw, 4, frame->ch_mode);
    }
    bitwriter_writebits(ctx->bw, 3, ctx->bps_code);
    bitwriter_writebits(ctx->bw, 1, 0);
    write_utf8(ctx->bw, ctx->frame_count);

    // custom block size
    if(frame->bs_code[1] >= 0) {
        if(frame->bs_code[1] < 256) {
            bitwriter_writebits(ctx->bw, 8, frame->bs_code[1]);
        } else {
            bitwriter_writebits(ctx->bw, 16, frame->bs_code[1]);
        }
    }

    // custom sample rate
    if(ctx->sr_code[1] > 0) {
        if(ctx->sr_code[1] < 256) {
            bitwriter_writebits(ctx->bw, 8, ctx->sr_code[1]);
        } else {
            bitwriter_writebits(ctx->bw, 16, ctx->sr_code[1]);
        }
    }

    // CRC-8 of frame header
    bitwriter_flush(ctx->bw);
    crc = calc_crc8(ctx->bw->buffer, bitwriter_count(ctx->bw));
    bitwriter_writebits(ctx->bw, 8, crc);
}

static void
output_residual(FlacEncodeContext *ctx, int ch)
{
    int i, j, p;
    int k, porder, psize, res_cnt, param_bits;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    // rice-encoded block
    bitwriter_writebits(ctx->bw, 2, sub->rc.method);

    // partition order
    porder = sub->rc.porder;
    psize = frame->blocksize >> porder;
    assert(porder >= 0);
    bitwriter_writebits(ctx->bw, 4, porder);
    res_cnt = psize - sub->order;

    // residual
    param_bits = 4 + sub->rc.method;
    j = sub->order;
    for(p=0; p<(1 << porder); p++) {
        k = sub->rc.params[p];
        bitwriter_writebits(ctx->bw, param_bits, k);
        for(i=0; i<res_cnt && j<frame->blocksize; i++, j++) {
            bitwriter_write_rice_signed(ctx->bw, k, sub->residual[j]);
        }
        res_cnt = psize;
    }
}

static void
output_subframe_constant(FlacEncodeContext *ctx, int ch)
{
    FlacSubframe *sub;

    sub = &ctx->frame.subframes[ch];
    bitwriter_writebits_signed(ctx->bw, sub->obits, sub->residual[0]);
}

static void
output_subframe_verbatim(FlacEncodeContext *ctx, int ch)
{
    int i, n;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    n = frame->blocksize;

    for(i=0; i<n; i++) {
        bitwriter_writebits_signed(ctx->bw, sub->obits, sub->residual[i]);
    }
}

static void
output_subframe_fixed(FlacEncodeContext *ctx, int ch)
{
    int i;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    // warm-up samples
    for(i=0; i<sub->order; i++) {
        bitwriter_writebits_signed(ctx->bw, sub->obits, sub->residual[i]);
    }

    // residual
    output_residual(ctx, ch);
}

static void
output_subframe_lpc(FlacEncodeContext *ctx, int ch)
{
    int i, cbits;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    // warm-up samples
    for(i=0; i<sub->order; i++) {
        bitwriter_writebits_signed(ctx->bw, sub->obits, sub->residual[i]);
    }

    // LPC coefficients
    cbits = ctx->lpc_precision;
    bitwriter_writebits(ctx->bw, 4, cbits-1);
    bitwriter_writebits_signed(ctx->bw, 5, sub->shift);
    for(i=0; i<sub->order; i++) {
        bitwriter_writebits_signed(ctx->bw, cbits, sub->coefs[i]);
    }

    // residual
    output_residual(ctx, ch);
}

static void
output_subframes(FlacEncodeContext *ctx)
{
    FlacFrame *frame;
    int i, ch;

    frame = &ctx->frame;

    for(i=0; i<ctx->channels; i++) {
        ch = i;

        // subframe header
        bitwriter_writebits(ctx->bw, 1, 0);
        bitwriter_writebits(ctx->bw, 6, frame->subframes[ch].type_code);
        if (frame->subframes[ch].wasted_bits) {
            bitwriter_writebits(ctx->bw, 1, 1);
            bitwriter_writebits(ctx->bw, frame->subframes[ch].wasted_bits-1, 0);
            bitwriter_writebits(ctx->bw, 1, 1);
        } else {
            bitwriter_writebits(ctx->bw, 1, 0);
        }

        // subframe
        switch(frame->subframes[ch].type) {
            case FLAC_SUBFRAME_CONSTANT: output_subframe_constant(ctx, ch);
                                         break;
            case FLAC_SUBFRAME_VERBATIM: output_subframe_verbatim(ctx, ch);
                                         break;
            case FLAC_SUBFRAME_FIXED:    output_subframe_fixed(ctx, ch);
                                         break;
            case FLAC_SUBFRAME_LPC:      output_subframe_lpc(ctx, ch);
                                         break;
        }
    }
}

static void
output_frame_footer(FlacEncodeContext *ctx)
{
    uint16_t crc;
    bitwriter_flush(ctx->bw);
    if(ctx->bw->eof)
        return;
    crc = calc_crc16(ctx->bw->buffer, bitwriter_count(ctx->bw));
    bitwriter_writebits(ctx->bw, 16, crc);
    bitwriter_flush(ctx->bw);
}

int
encode_frame(FlacEncodeContext *ctx, uint8_t *frame_buffer, int buf_size,
             const int32_t *samples, int block_size)
{
    int i, ch;

    if(!ctx || !samples || buf_size <= 0)
        return -1;

    if(init_frame(ctx, block_size)) {
        return -1;
    }

    copy_samples(ctx, samples);

    channel_decorrelation(ctx);

    remove_wasted_bits(ctx);

    for(ch=0; ch<ctx->channels; ch++) {
        if(encode_residual(ctx, ch) < 0) {
            return -1;
        }
    }

    bitwriter_init(ctx->bw, frame_buffer, buf_size);
    output_frame_header(ctx);
    output_subframes(ctx);
    output_frame_footer(ctx);

    if(ctx->bw->eof || bitwriter_count(ctx->bw) > ctx->frame.verbatim_size) {
        // frame size too large, reencode in verbatim mode
        for(i=0; i<ctx->channels; i++) {
            ch = i;
            reencode_residual_verbatim(ctx, ch);
        }
        bitwriter_init(ctx->bw, frame_buffer, buf_size);
        output_frame_header(ctx);
        output_subframes(ctx);
        output_frame_footer(ctx);

        // if still too large, means my estimate is wrong
        // or the sum of vbs frames is too large
        if(ctx->bw->eof)
            return -1;
    }

    // update maximum frame size
    ctx->max_frame_size = MAX(ctx->max_frame_size, bitwriter_count(ctx->bw));

    if(frame_buffer != NULL) {
        if(ctx->params.variable_block_size || ctx->params.allow_vbs) {
            ctx->frame_count += ctx->frame.blocksize;
        } else {
            ctx->frame_count++;
        }
    }
    return bitwriter_count(ctx->bw);
}

int
flake_encode_frame(FlakeContext *s, const int *samples, int block_size)
{
    int fs;
    FlacEncodeContext *ctx;

    if(!s || !samples || !s->private_ctx)
        return -1;
    ctx = (FlacEncodeContext *) s->private_ctx;

    if(block_size < 1 || block_size > ctx->params.block_size)
        return -1;
    if(ctx->last_frame)
        return -1;
    if(!ctx->params.allow_vbs && block_size != ctx->params.block_size)
        ctx->last_frame = 1;

    fs = -1;
    if((ctx->params.variable_block_size > 0) &&
       !(block_size % VBS_MAX_FRAMES) && block_size >= VBS_MIN_BLOCK_SIZE) {
        fs = encode_frame_vbs(ctx, samples, block_size);
    }
    if(fs < 0) {
        fs = encode_frame(ctx, ctx->frame_buffer, ctx->frame_buffer_size, samples,
                          block_size);
    }
    if(fs > 0)
        md5_accumulate(&ctx->md5ctx, samples, ctx->channels, ctx->bps, block_size);
    return fs;
}

void
flake_encode_close(FlakeContext *s)
{
    FlacEncodeContext *ctx;

    if(s == NULL) return;
    if(s->private_ctx == NULL) return;
    ctx = (FlacEncodeContext *) s->private_ctx;
    if(ctx) {
        if(ctx->bw) free(ctx->bw);
        if(ctx->frame_buffer) free(ctx->frame_buffer);
        md5_close(&ctx->md5ctx);
        free(ctx);
    }
    if(s->header) free(s->header);
    s->private_ctx = NULL;
}

const char *
flake_get_version(void)
{
#ifdef SVN_VERSION
    static const char *const str = FLAKE_VERSION "-r" SVN_VERSION;
#else
    static const char *const str = FLAKE_VERSION;
#endif

    return str;
}
