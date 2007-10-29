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

/**
 * @file metadata.c
 * FLAC Metadata functions
 */

#include "common.h"

#include "flake.h"
#include "bitio.h"
#include "encode.h"
#include "md5.h"

int
flake_metadata_get_streaminfo(const FlakeContext *s, FlakeStreaminfo *strminfo)
{
    FlacEncodeContext *ctx;
    MD5Context md5_bak;

    if(!s || !strminfo)
        return -1;

    if(flake_validate_params(s) < 0)
        return -1;

    ctx = s->private_ctx;

    // get parameters from encoding context
    if(ctx->params.variable_block_size || ctx->params.allow_vbs) {
        strminfo->min_block_size = 16;
    } else {
        strminfo->min_block_size = ctx->params.block_size;
    }
    strminfo->max_block_size    = ctx->params.block_size;
    strminfo->min_frame_size    = 0;
    strminfo->max_frame_size    = ctx->max_frame_size;
    strminfo->sample_rate       = ctx->samplerate;
    strminfo->channels          = ctx->channels;
    strminfo->bits_per_sample   = ctx->bps;
    strminfo->samples           = ctx->sample_count;

    // get MD5 checksum
    md5_bak = ctx->md5ctx;
    md5_final(strminfo->md5sum, &md5_bak);

    return 0;
}

void
flake_metadata_write_streaminfo(const FlakeStreaminfo *strminfo, uint8_t *data)
{
    BitWriter bw;
    memset(data, 0, 34);
    bitwriter_init(&bw, data, 34);
    bitwriter_writebits(&bw, 16, strminfo->min_block_size);
    bitwriter_writebits(&bw, 16, strminfo->max_block_size);
    bitwriter_writebits(&bw, 24, strminfo->min_frame_size);
    bitwriter_writebits(&bw, 24, strminfo->max_frame_size);
    bitwriter_writebits(&bw, 20, strminfo->sample_rate);
    bitwriter_writebits(&bw,  3, strminfo->channels-1);
    bitwriter_writebits(&bw,  5, strminfo->bits_per_sample-1);
    bitwriter_writebits(&bw,  4, 0);
    bitwriter_writebits(&bw, 32, strminfo->samples);
    bitwriter_flush(&bw);
    memcpy(&data[18], strminfo->md5sum, 16);
}
