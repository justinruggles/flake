/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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

/**
 * @file wav.c
 * WAV decoder
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "wav.h"
#include "bswap.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define CLIP(x,min,max) MAX(MIN((x), (max)), (min))

#define RIFF_ID     0x46464952
#define WAVE_ID     0x45564157
#define FMT__ID     0x20746D66
#define DATA_ID     0x61746164

static inline uint32_t
read4le(FILE *fp)
{
    uint32_t x;
    fread(&x, 4, 1, fp);
    return le2me_32(x);
}

static inline uint16_t
read2le(FILE *fp)
{
    uint16_t x;
    fread(&x, 2, 1, fp);
    return le2me_16(x);
}

int
wavfile_init(WavFile *wf, FILE *fp)
{
    int id, chunksize, found_fmt, found_data;

    if(wf == NULL || fp == NULL) return -1;

    memset(wf, 0, sizeof(WavFile));
    wf->fp = fp;

    // attempt to get file size
    wf->file_size = 0;
    wf->seekable = !fseek(fp, 0, SEEK_END);
    if(wf->seekable) {
        wf->file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    wf->filepos = 0;
    id = read4le(fp);
    wf->filepos += 4;
    if(id != RIFF_ID) return -1;
    read4le(fp);
    wf->filepos += 4;
    id = read4le(fp);
    wf->filepos += 4;
    if(id != WAVE_ID) return -1;
    found_data = found_fmt = 0;
    while(!found_data) {
        id = read4le(fp);
        wf->filepos += 4;
        chunksize = read4le(fp);
        wf->filepos += 4;
        if(id == 0 || chunksize == 0) return -1;
        switch(id) {
            case FMT__ID:
                if(chunksize < 16) return -1;
                wf->format = read2le(fp);
                wf->channels = read2le(fp);
                wf->sample_rate = read4le(fp);
                read4le(fp);
                wf->block_align = read2le(fp);
                wf->bit_width = read2le(fp);
                wf->filepos += 16;
                if(!wf->channels || !wf->sample_rate || !wf->block_align || !wf->bit_width) {
                    return -1;
                }
                chunksize -= 16;

                // WAVE_FORMAT_EXTENSIBLE data
                wf->ch_mask = 0;
                if(wf->format == WAVE_FORMAT_EXTENSIBLE && chunksize >= 10) {
                    read4le(fp);    // skip CbSize and ValidBitsPerSample
                    wf->ch_mask = read4le(fp);
                    wf->format = read2le(fp);
                    wf->filepos += 10;
                    chunksize -= 10;
                }

                if(wf->format == WAVE_FORMAT_PCM || wf->format == WAVE_FORMAT_IEEEFLOAT) {
                    wf->block_align = ((wf->bit_width + 7) >> 3) * wf->channels;
                }

                // make up channel mask if not using WAVE_FORMAT_EXTENSIBLE
                // or if ch_mask is set to zero (unspecified configuration)
                // TODO: select default configurations for >6 channels
                if(wf->ch_mask == 0) {
                    switch(wf->channels) {
                        case 1: wf->ch_mask = 0x04;  break;
                        case 2: wf->ch_mask = 0x03;  break;
                        case 3: wf->ch_mask = 0x07;  break;
                        case 4: wf->ch_mask = 0x107; break;
                        case 5: wf->ch_mask = 0x37;  break;
                        case 6: wf->ch_mask = 0x3F;  break;
                    }
                }

                while(chunksize-- > 0) {
                    fgetc(fp);
                    wf->filepos++;
                }
                found_fmt = 1;
                break;
            case DATA_ID:
                if(!found_fmt) return -1;
                wf->data_size = chunksize;
                wf->data_start = wf->filepos;
                wf->samples = (wf->data_size / wf->block_align);
                found_data = 1;
                break;
            default:
                if(wf->seekable) {
                    fseek(fp, chunksize, SEEK_CUR);
                } else {
                    while(chunksize-- > 0) {
                        fgetc(fp);
                    }
                }
                wf->filepos += chunksize;
        }
    }

    wf->source_format = WAV_SAMPLE_FMT_UNKNOWN;
    wf->read_format = wf->source_format;
    if(wf->format == WAVE_FORMAT_PCM || wf->format == WAVE_FORMAT_IEEEFLOAT) {
        switch(wf->bit_width) {
            case 8:  wf->source_format = WAV_SAMPLE_FMT_U8;   break;
            case 16:  wf->source_format = WAV_SAMPLE_FMT_S16; break;
            case 20:  wf->source_format = WAV_SAMPLE_FMT_S20; break;
            case 24:  wf->source_format = WAV_SAMPLE_FMT_S24; break;
            case 32:
                if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
                    wf->source_format = WAV_SAMPLE_FMT_FLT;
                } else if(wf->format == WAVE_FORMAT_PCM) {
                    wf->source_format = WAV_SAMPLE_FMT_S32;
                }
                break;
            case 64:
                if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
                    wf->source_format = WAV_SAMPLE_FMT_DBL;
                }
                break;
        }
    }

    return 0;
}

static void
fmt_convert_to_u8(uint8_t *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i, v;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        memcpy(dest, src_v, n);
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 8) + 128;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 12) + 128;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 16) + 128;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 24) + 128;
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP(((src[i] * 128) + 128), 0, 255);
            dest[i] = v;
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP(((src[i] * 128) + 128), 0, 255);
            dest[i] = v;
        }
    }
}

static void
fmt_convert_to_s16(int16_t *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i, v;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128) << 8;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        memcpy(dest, src_v, n * sizeof(int16_t));
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 4);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 8);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 16);
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 32768), -32768, 32767);
            dest[i] = v;
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 32768), -32768, 32767);
            dest[i] = v;
        }
    }
}

static void
fmt_convert_to_s20(int32_t *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i, v;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128) << 12;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 4);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        memcpy(dest, src_v, n * sizeof(int32_t));
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 4);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 12);
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 524288), -524288, 524287);
            dest[i] = v;
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 524288), -524288, 524287);
            dest[i] = v;
        }
    }
}

static void
fmt_convert_to_s24(int32_t *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i, v;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128) << 16;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 8);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 4);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        memcpy(dest, src_v, n * sizeof(int32_t));
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] >> 8);
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 8388608), -8388608, 8388607);
            dest[i] = v;
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            v = CLIP((src[i] * 8388608), -8388608, 8388607);
            dest[i] = v;
        }
    }
}

static void
fmt_convert_to_s32(int32_t *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i, v;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128) << 24;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 16);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 12);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] << 8);
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        memcpy(dest, src_v, n * sizeof(int32_t));
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            v = (src[i] * 2147483648LL);
            dest[i] = v;
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            v = (src[i] * 2147483648LL);
            dest[i] = v;
        }
    }
}

static void
fmt_convert_to_float(float *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128.0) / 128.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 32768.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 524288.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 8388608.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 2147483648.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        memcpy(dest, src_v, n * sizeof(float));
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        double *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i];
        }
    }
}

static void
fmt_convert_to_double(double *dest, void *src_v, int n, enum WavSampleFormat fmt)
{
    int i;

    if(fmt == WAV_SAMPLE_FMT_U8) {
        uint8_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = (src[i] - 128.0) / 128.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S16) {
        int16_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 32768.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S20) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 524288.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S24) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 8388608.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_S32) {
        int32_t *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i] / 2147483648.0;
        }
    } else if(fmt == WAV_SAMPLE_FMT_FLT) {
        float *src = src_v;
        for(i=0; i<n; i++) {
            dest[i] = src[i];
        }
    } else if(fmt == WAV_SAMPLE_FMT_DBL) {
        memcpy(dest, src_v, n * sizeof(double));
    }
}

static void
fmt_convert(enum WavSampleFormat dest_fmt, void *dest,
            enum WavSampleFormat src_fmt, void *src, int n)
{
    switch(dest_fmt) {
        case WAV_SAMPLE_FMT_U8:
            fmt_convert_to_u8(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_S16:
            fmt_convert_to_s16(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_S20:
            fmt_convert_to_s20(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_S24:
            fmt_convert_to_s24(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_S32:
            fmt_convert_to_s32(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_FLT:
            fmt_convert_to_float(dest, src, n, src_fmt);
            break;
        case WAV_SAMPLE_FMT_DBL:
            fmt_convert_to_double(dest, src, n, src_fmt);
            break;
    }
}

int
wavfile_read_samples(WavFile *wf, void *output, int num_samples)
{
    int nr, i, j, bps, nsmp, v;
    int convert = 1;
    uint8_t *buffer;

    if(wf == NULL || wf->fp == NULL || output == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(num_samples < 0) return -1;
    if(num_samples == 0) return 0;

    convert = (wf->read_format != wf->source_format);
    if(convert) {
        buffer = calloc(wf->block_align * num_samples, 1);
    } else {
        buffer = output;
    }

    nr = fread(buffer, wf->block_align, num_samples, wf->fp);
    wf->filepos += nr * wf->block_align;
    nsmp = nr * wf->channels;

    bps = wf->block_align / wf->channels;
    if(bps == 1) {
        if(wf->source_format != WAV_SAMPLE_FMT_U8) return -1;
        if(convert) {
            fmt_convert(wf->read_format, output, wf->source_format, buffer, nsmp);
        }
    } else if(bps == 2) {
#ifdef WORDS_BIGENDIAN
        uint16_t *buf16 = (uint16_t *)buffer;
        for(i=0; i<nsmp; i++) {
            buf16[i] = bswap_16(buf16[i]);
        }
#endif
        if(wf->source_format != WAV_SAMPLE_FMT_S16) return -1;
        if(convert) {
            fmt_convert(wf->read_format, output, wf->source_format, (int16_t *)buffer, nsmp);
        }
    } else if(bps == 3) {
        int32_t *input = calloc(nsmp, sizeof(int32_t));
        for(i=0,j=0; i<nsmp*bps; i+=bps,j++) {
            v = buffer[i] + (buffer[i+1] << 8) + (buffer[i+2] << 16);
            if(wf->bit_width == 20) {
                if(v >= (1<<19)) v -= (1<<20);
            } else if(wf->bit_width == 24) {
                if(v >= (1<<23)) v -= (1<<24);
            } else {
                fprintf(stderr, "unsupported bit width: %d\n", wf->bit_width);
                return -1;
            }
            input[j] = v;
        }
        if(wf->source_format != WAV_SAMPLE_FMT_S20 &&
                wf->source_format != WAV_SAMPLE_FMT_S24) {
            return -1;
        }
        if(convert) {
            fmt_convert(wf->read_format, output, wf->source_format, input, nsmp);
        }
        free(input);
    } else if(bps == 4) {
#ifdef WORDS_BIGENDIAN
        uint32_t *buf32 = (uint32_t *)buffer;
        for(i=0; i<nsmp; i++) {
            buf32[i] = bswap_32(buf32[i]);
        }
#endif
        if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
            if(wf->source_format != WAV_SAMPLE_FMT_FLT) return -1;
            if(convert) {
                fmt_convert(wf->read_format, output, wf->source_format, (float *)buffer, nsmp);
            }
        } else {
            if(wf->source_format != WAV_SAMPLE_FMT_S32) return -1;
            if(convert) {
                fmt_convert(wf->read_format, output, wf->source_format, (int32_t *)buffer, nsmp);
            }
        }
    } else if(wf->format == WAVE_FORMAT_IEEEFLOAT && bps == 8) {
#ifdef WORDS_BIGENDIAN
        uint64_t *buf64 = (uint64_t *)buffer;
        for(i=0; i<nsmp; i++) {
            buf64[i] = bswap_64(buf64[i]);
        }
#endif
        if(wf->source_format != WAV_SAMPLE_FMT_DBL) return -1;
        if(convert) {
            fmt_convert(wf->read_format, output, wf->source_format, (double *)buffer, nsmp);
        }
    }
    if(convert) {
        free(buffer);
    }

    return nr;
}

int
wavfile_seek_samples(WavFile *wf, int32_t offset, int whence)
{
    int byte_offset, pos, cur;

    if(wf == NULL || wf->fp == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(wf->data_start == 0 || wf->data_size == 0) return -1;
    byte_offset = offset * wf->block_align;
    pos = wf->data_start;
    switch(whence) {
        case WAV_SEEK_SET:
            pos = wf->data_start + byte_offset;
            break;
        case WAV_SEEK_CUR:
            cur = wf->filepos;
            while(cur < wf->data_start) {
                fgetc(wf->fp);
                cur++;
                wf->filepos++;
            }
            pos = cur + byte_offset;
        case WAV_SEEK_END:
            pos = (wf->data_start+wf->data_size) - byte_offset;
            break;
        default: return -1;
    }
    if(pos < wf->data_start) {
        pos = 0;
    }
    if(pos >= wf->data_start+wf->data_size) {
        pos = wf->data_start+wf->data_size-1;
    }
    if(!wf->seekable) {
        if(pos < wf->filepos) return -1;
        while(wf->filepos < pos) {
            fgetc(wf->fp);
            wf->filepos++;
        }
    } else {
        if(fseek(wf->fp, pos, SEEK_SET)) return -1;
    }
    return 0;
}

int
wavfile_seek_time_ms(WavFile *wf, int32_t offset, int whence)
{
    int32_t samples;
    if(wf == NULL || wf->sample_rate == 0) return -1;
    samples = (offset * wf->sample_rate) / 1000;
    return wavfile_seek_samples(wf, samples, whence);
}

int
wavfile_position(WavFile *wf)
{
    int cur;

    if(wf == NULL) return 0;
    if(wf->data_start == 0 || wf->data_size == 0) return 0;

    cur = (wf->filepos - wf->data_start) / wf->block_align;
    if(cur <= 0) return 0;
    cur /= wf->block_align;
    return cur;
}

void
wavfile_print(FILE *st, WavFile *wf)
{
    char *type, *chan;
    if(st == NULL || wf == NULL) return;
    if(wf->format == WAVE_FORMAT_PCM) {
        if(wf->bit_width > 8) type = "Signed";
        else type = "Unsigned";
    } else if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
        type = "Floating-point";
    } else {
        type = "[unsupported type]";
    }
    switch(wf->channels) {
        case 1: chan = "mono"; break;
        case 2: chan = "stereo"; break;
        case 3: chan = "3-channel"; break;
        case 4: chan = "4-channel"; break;
        case 5: chan = "5-channel"; break;
        case 6: chan = "6-channel"; break;
        default: chan = "multi-channel"; break;
    }
    fprintf(st, "%s %d-bit %d Hz %s\n", type, wf->bit_width, wf->sample_rate,
            chan);
}
