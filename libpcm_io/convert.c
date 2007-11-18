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
fmt_convert_float_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (uint8_t)CLIP(((src[i] * 128) + 128), 0, 255);
}

static void
fmt_convert_double_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (uint8_t)CLIP(((src[i] * 128) + 128), 0, 255);
}

static void
fmt_convert_u8_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 8;
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
fmt_convert_float_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int16_t)CLIP((src[i] * 32768), -32768, 32767);
}

static void
fmt_convert_double_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int16_t)CLIP((src[i] * 32768), -32768, 32767);
}

static void
fmt_convert_u8_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 12;
}

static void
fmt_convert_s16_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 4);
}

static void
fmt_convert_s20_to_s20(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s24_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 4);
}

static void
fmt_convert_s32_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 12);
}

static void
fmt_convert_float_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 524288), -524288, 524287);
}

static void
fmt_convert_double_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 524288), -524288, 524287);
}

static void
fmt_convert_u8_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 16;
}

static void
fmt_convert_s16_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 8);
}

static void
fmt_convert_s20_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 4);
}

static void
fmt_convert_s24_to_s24(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s32_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8);
}

static void
fmt_convert_float_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 8388608), -8388608, 8388607);
}

static void
fmt_convert_double_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 8388608), -8388608, 8388607);
}

static void
fmt_convert_u8_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 24;
}

static void
fmt_convert_s16_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 16);
}

static void
fmt_convert_s20_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 12);
}

static void
fmt_convert_s24_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 8);
}

static void
fmt_convert_s32_to_s32(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_float_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)(src[i] * 2147483648LL);
}

static void
fmt_convert_double_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)(src[i] * 2147483648LL);
}

static void
fmt_convert_u8_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128.0f) / 128.0f;
}

static void
fmt_convert_s16_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 32768.0f;
}

static void
fmt_convert_s20_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 524288.0f;
}

static void
fmt_convert_s24_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 8388608.0f;
}

static void
fmt_convert_s32_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 2147483648.0f;
}

static void
fmt_convert_float_to_float(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(float));
}

static void
fmt_convert_double_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (float)src[i];
}

static void
fmt_convert_u8_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128.0) / 128.0;
}

static void
fmt_convert_s16_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 32768.0;
}

static void
fmt_convert_s20_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 524288.0;
}

static void
fmt_convert_s24_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 8388608.0;
}

static void
fmt_convert_s32_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / 2147483648.0;
}

static void
fmt_convert_float_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i];
}

static void
fmt_convert_double_to_double(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(double));
}

static void
set_fmt_convert_from_u8(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_u8_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_u8_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_u8_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_u8_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_u8_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_u8_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_u8_to_double;
}

static void
set_fmt_convert_from_s16(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_s16_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_s16_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_s16_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_s16_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_s16_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_s16_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_s16_to_double;
}

static void
set_fmt_convert_from_s20(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_s20_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_s20_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_s20_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_s20_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_s20_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_s20_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_s20_to_double;
}

static void
set_fmt_convert_from_s24(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_s24_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_s24_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_s24_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_s24_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_s24_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_s24_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_s24_to_double;
}

static void
set_fmt_convert_from_s32(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_s32_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_s32_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_s32_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_s32_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_s32_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_s32_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_s32_to_double;
}

static void
set_fmt_convert_from_float(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_float_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_float_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_float_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_float_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_float_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_float_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_float_to_double;
}

static void
set_fmt_convert_from_double(PcmFile *pf)
{
    enum PcmSampleFormat fmt = pf->read_format;

    if(fmt == PCM_SAMPLE_FMT_U8)
        pf->fmt_convert = fmt_convert_double_to_u8;
    else if(fmt == PCM_SAMPLE_FMT_S16)
        pf->fmt_convert = fmt_convert_double_to_s16;
    else if(fmt == PCM_SAMPLE_FMT_S20)
        pf->fmt_convert = fmt_convert_double_to_s20;
    else if(fmt == PCM_SAMPLE_FMT_S24)
        pf->fmt_convert = fmt_convert_double_to_s24;
    else if(fmt == PCM_SAMPLE_FMT_S32)
        pf->fmt_convert = fmt_convert_double_to_s32;
    else if(fmt == PCM_SAMPLE_FMT_FLT)
        pf->fmt_convert = fmt_convert_double_to_float;
    else if(fmt == PCM_SAMPLE_FMT_DBL)
        pf->fmt_convert = fmt_convert_double_to_double;
}

static const int format_bps[7] = { 8, 16, 20, 24, 32, 32, 64 };

void
pcmfile_set_source_format(PcmFile *pf, int fmt)
{
    switch(fmt) {
        case PCM_SAMPLE_FMT_U8:
            set_fmt_convert_from_u8(pf);
            break;
        case PCM_SAMPLE_FMT_S16:
            set_fmt_convert_from_s16(pf);
            break;
        case PCM_SAMPLE_FMT_S20:
            set_fmt_convert_from_s20(pf);
            break;
        case PCM_SAMPLE_FMT_S24:
            set_fmt_convert_from_s24(pf);
            break;
        case PCM_SAMPLE_FMT_S32:
            set_fmt_convert_from_s32(pf);
            break;
        case PCM_SAMPLE_FMT_FLT:
            set_fmt_convert_from_float(pf);
            break;
        case PCM_SAMPLE_FMT_DBL:
            set_fmt_convert_from_double(pf);
            break;
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
