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

#include "flake.h"
#include "vbs.h"
#include "encode.h"

#define SPLIT_THRESHOLD 50

/**
 * Split single frame into smaller frames using predictability comparison.
 * This algorithm computes predictablity estimates for sections of the frame
 * by first summing the absolute value of fixed 2nd order residual, averaged
 * across all channels.  The predictability is compared between adjacent
 * sections to determine if they should be merged based on a fixed comparison
 * threshold.
 */
static void
split_frame_v1(const int32_t *samples, int channels, int block_size,
               int *frames, int sizes[VBS_MAX_FRAMES])
{
    int i, ch, j;
    int n = block_size / VBS_MAX_FRAMES;
    int64_t res[VBS_MAX_FRAMES];
    int layout[VBS_MAX_FRAMES];
    const int32_t *sptr, *sptr0, *sptr1, *sptr2;

    // calculate absolute sum of 2nd order residual
    for(i=0; i<VBS_MAX_FRAMES; i++) {
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
    memset(layout, 0, VBS_MAX_FRAMES * sizeof(int));
    layout[0] = 1;
    for(i=1; i<VBS_MAX_FRAMES; i++) {
        if(abs(res[i-1]-res[i])*200 / res[i-1] > SPLIT_THRESHOLD) {
            layout[i] = 1;
        }
    }

    // generate frame count and frame sizes from layout
    frames[0] = 0;
    memset(sizes, 0, VBS_MAX_FRAMES * sizeof(int));
    for(i=0; i<VBS_MAX_FRAMES; i++) {
        if(layout[i]) {
            frames[0]++;
        }
        sizes[frames[0]-1] += n;
    }
}

int
encode_frame_vbs(FlacEncodeContext *ctx, const int32_t *samples, int block_size)
{
    int fs = -1;
    int frames;
    int sizes[VBS_MAX_FRAMES];
    int fc0;

    if(!ctx || !samples || block_size < VBS_MIN_BLOCK_SIZE || block_size % VBS_MAX_FRAMES)
        return -1;

    fc0 = ctx->frame_count;

    split_frame_v1(samples, ctx->channels, block_size, &frames, sizes);

    if(frames > 1) {
        int i, fpos, spos;
        fpos = 0;
        spos = 0;
        for(i=0; i<frames; i++) {
            fs = encode_frame(ctx, &ctx->frame_buffer[fpos],
                              ctx->frame_buffer_size-fpos,
                              &samples[spos*ctx->channels], sizes[i]);
            if(fs < 0) {
                ctx->frame_count = fc0;
                return -1;
            }
            fpos += fs;
            spos += sizes[i];
        }
        assert(spos == block_size);
        return fpos;
    }
    return fs;
}
