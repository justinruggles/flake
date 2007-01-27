/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006  Justin Ruggles
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

#include "common.h"

#include "flake.h"
#include "vbs.h"
#include "encode.h"

#define SPLIT_THRESHOLD 100

/**
 * Split single frame into smaller frames using predictability comparison.
 * This algorithm computes predictablity estimates for sections of the frame
 * by first summing the absolute value of fixed 2nd order residual, averaged
 * across all channels.  The predictability is compared between adjacent
 * sections to determine if they should be merged based on a fixed comparison
 * threshold.
 */
static void
split_frame_v1(int16_t *samples, int channels, int block_size,
               int *frames, int sizes[8])
{
    int i, ch, j;
    int n = block_size >> 3;
    int64_t res[8];
    int layout[8];
    int16_t *sptr, *sptr0, *sptr1, *sptr2;

    // calculate absolute sum of 2nd order residual
    for(i=0; i<8; i++) {
        sptr = &samples[i*n*channels];
        res[i] = 0;
        for(ch=0; ch<channels; ch++) {
            sptr0 = sptr + (2*channels+ch);
            sptr1 = sptr0 - channels;
            sptr2 = sptr1 - channels;
            for(j=2; j<n; j++) {
                res[i] += abs((*sptr0) - 2*(*sptr1) + (*sptr2));
                sptr0 += channels;
                sptr1 += channels;
                sptr2 += channels;
            }
        }
        res[i] /= channels;
        res[i]++;
    }

    // determine frame layout
    memset(layout, 0, 8 * sizeof(int));
    layout[0] = 1;
    for(i=0; i<7; i++) {
        if(abs(res[i]-res[i+1])*200 / res[i] > SPLIT_THRESHOLD) {
            layout[i+1] = 1;
        }
    }

    // generate frame count and frame sizes from layout
    frames[0] = 0;
    memset(sizes, 0, 8 * sizeof(int));
    for(i=0; i<8; i++) {
        if(layout[i]) {
            frames[0]++;
        }
        sizes[frames[0]-1] += n;
    }
}

static void
split_frame_v2(FlakeContext *s, int16_t *samples, int *frames, int sizes[8])
{
    int fsizes[4][8];
    int layout[8];
    int i, j, n, ch;
    FlacEncodeContext *ctx = (FlacEncodeContext *) s->private_ctx;
    ch = ctx->channels;

    // encode for each level to get sizes
    for(i=0; i<4; i++) {
        int levels, bs;
        levels = (1<<i);
        s->params.block_size /= levels;
        bs = s->params.block_size;
        for(j=0; j<levels; j++) {
            fsizes[i][j] = encode_frame(s, NULL, &samples[bs*j*ch]);
        }
        s->params.block_size *= levels;
    }

    // initialize layout
    for(i=0; i<8; i++) layout[i] = 1;
    // level 3 merge
    if(fsizes[2][0] < (fsizes[3][0]+fsizes[3][1])) {
        layout[1] = 0;
    }
    if(fsizes[2][1] < (fsizes[3][2]+fsizes[3][3])) {
        layout[3] = 0;
    }
    if(fsizes[2][2] < (fsizes[3][4]+fsizes[3][5])) {
        layout[5] = 0;
    }
    if(fsizes[2][3] < (fsizes[3][6]+fsizes[3][7])) {
        layout[7] = 0;
    }
    // level 2 merge
    if(layout[1] == 0 && layout[3] == 0) {
        if(fsizes[1][0] < (fsizes[2][0]+fsizes[2][1])) {
            layout[2] = 0;
        }
    }
    if(layout[5] == 0 && layout[7] == 0) {
        if(fsizes[1][1] < (fsizes[2][2]+fsizes[2][3])) {
            layout[6] = 0;
        }
    }
    // level 1 merge
    if(layout[2] == 0 && layout[6] == 0) {
        if(fsizes[0][0] < (fsizes[1][0]+fsizes[1][1])) {
            layout[4] = 0;
        }
    }

    // generate frame count and frame sizes from layout
    n = s->params.block_size >> 3;
    frames[0] = 0;
    memset(sizes, 0, 8 * sizeof(int));
    for(i=0; i<8; i++) {
        if(layout[i]) {
            frames[0]++;
        }
        sizes[frames[0]-1] += n;
    }
}

int
encode_frame_vbs(FlakeContext *s, uint8_t *frame_buffer, int16_t *samples)
{
    int fs;
    int frames;
    int sizes[8];
    FlacEncodeContext *ctx;

    ctx = (FlacEncodeContext *) s->private_ctx;

    switch(ctx->params.variable_block_size) {
        case 1: split_frame_v1(samples, s->channels, s->params.block_size, &frames, sizes);
                break;
        case 2: split_frame_v2(s, samples, &frames, sizes);
                break;
        default: frames = 1;
                break;
    }
    if(frames > 1) {
        int i, fpos, spos, bs;
        fpos = 0;
        spos = 0;
        bs = s->params.block_size;
        for(i=0; i<frames; i++) {
            s->params.block_size = sizes[i];
            fs = encode_frame(s, &frame_buffer[fpos], &samples[spos*ctx->channels]);
            if(fs < 0) return -1;
            fpos += fs;
            spos += sizes[i];
        }
        s->params.block_size = bs;
        assert(spos == bs);
        return fpos;
    }
    fs = encode_frame(s, frame_buffer, samples);
    return fs;
}
