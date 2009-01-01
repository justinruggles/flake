/**
 * libpcm_io: Raw PCM Audio I/O library
 * Copyright (c) 2006-2007 Justin Ruggles
 *
 * libpcm_io is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libpcm_io is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libpcm_io; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file convert.c
 * Raw audio sample format conversion
 */

#include <string.h>

#include "pcm_io_common.h"
#include "pcm_io.h"

static void
fmt_convert_u8_to_u8(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(uint8_t));
}

static void
fmt_convert_s16_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8) + 128;
}

static void
fmt_convert_s20_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 12) + 128;
}

static void
fmt_convert_s24_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 16) + 128;
}

static void
fmt_convert_s32_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 24) + 128;
}

static void
fmt_convert_u8_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int)src[i] - 128;
}

static void
fmt_convert_s16_to_s16(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int16_t));
}

static void
fmt_convert_s20_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 4);
}

static void
fmt_convert_s24_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8);
}

static void
fmt_convert_s32_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 16);
}

static void
fmt_convert_u8_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int)src[i] - 128;
}

static void
fmt_convert_s16_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i];
}

static void
fmt_convert_s20_to_s32(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s24_to_s32(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s32_to_s32(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}


#define SET_FMT_CONVERT_FROM(srcfmt, pf) \
{ \
    enum PcmSampleFormat fmt = pf->read_format; \
 \
    if(fmt == PCM_SAMPLE_FMT_U8) \
        pf->fmt_convert = fmt_convert_##srcfmt##_to_u8; \
    else if(fmt == PCM_SAMPLE_FMT_S16) \
        pf->fmt_convert = fmt_convert_##srcfmt##_to_s16; \
    else if(fmt == PCM_SAMPLE_FMT_S32) \
        pf->fmt_convert = fmt_convert_##srcfmt##_to_s32; \
}

static const int format_bps[7] = { 8, 16, 20, 24, 32, 32, 64 };

void
pcmfile_set_source_format(PcmFile *pf, enum PcmSampleFormat fmt)
{
    switch(fmt) {
        case PCM_SAMPLE_FMT_U8:  SET_FMT_CONVERT_FROM(u8,     pf); break;
        case PCM_SAMPLE_FMT_S16: SET_FMT_CONVERT_FROM(s16,    pf); break;
        case PCM_SAMPLE_FMT_S20: SET_FMT_CONVERT_FROM(s20,    pf); break;
        case PCM_SAMPLE_FMT_S24: SET_FMT_CONVERT_FROM(s24,    pf); break;
        case PCM_SAMPLE_FMT_S32: SET_FMT_CONVERT_FROM(s32,    pf); break;
        default:
            pf->source_format = PCM_SAMPLE_FMT_UNKNOWN;
            pf->fmt_convert = NULL;
            return;
    }
    pf->source_format = fmt;
    pf->bit_width = format_bps[fmt];
    pf->block_align = MAX(1, ((pf->bit_width + 7) >> 3) * pf->channels);
    pf->samples = (pf->data_size / pf->block_align);
}

void
pcmfile_set_source_params(PcmFile *pf, int ch, enum PcmSampleFormat fmt, int order, int sr)
{
    pf->channels = MAX(ch, 1);
    pf->ch_mask = pcmfile_get_default_ch_mask(ch);
    pf->order = CLIP(order, PCM_BYTE_ORDER_LE, PCM_BYTE_ORDER_BE);
    pf->sample_rate = MAX(sr, 1);
    pcmfile_set_source_format(pf, fmt);
}

void
pcmfile_set_read_format(PcmFile *pf, enum PcmDataFormat read_format)
{
    pf->read_format = CLIP(read_format, PCM_DATA_FORMAT_U8, PCM_DATA_FORMAT_S32);
    pcmfile_set_source_format(pf, pf->source_format);
}
