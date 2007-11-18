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
 * @file wav.c
 * WAV file format
 */

#include "bswap.h"
#include "pcm_io_common.h"
#include "pcm_io.h"

/* chunk id's */
#define RIFF_ID     0x46464952
#define WAVE_ID     0x45564157
#define FMT__ID     0x20746D66
#define DATA_ID     0x61746164

/**
 * Reads a 4-byte little-endian word from the input stream
 */
static inline uint32_t
read4le(PcmFile *pf)
{
    uint32_t x;
    if(byteio_read(&x, 4, &pf->io) != 4)
        return 0;
    pf->filepos += 4;
    return le2me_32(x);
}

/**
 * Reads a 2-byte little-endian word from the input stream
 */
static inline uint16_t
read2le(PcmFile *pf)
{
    uint16_t x;
    if(byteio_read(&x, 2, &pf->io) != 2)
        return 0;
    pf->filepos += 2;
    return le2me_16(x);
}

int
pcmfile_probe_wave(uint8_t *data, int size)
{
    int id;

    if(!data || size < 12)
        return 0;
    id = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    if(id != RIFF_ID) {
        return 0;
    }
    id = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
    if(id != WAVE_ID) {
        return 0;
    }
    return 100;
}

int
pcmfile_init_wave(PcmFile *pf)
{
    int id, found_data, found_fmt, chunksize;

    // read RIFF id. ignore size.
    id = read4le(pf);
    if(id != RIFF_ID) {
        fprintf(stderr, "invalid RIFF id in wav header\n");
        return -1;
    }
    read4le(pf);

    // read WAVE id. ignore size.
    id = read4le(pf);
    if(id != WAVE_ID) {
        fprintf(stderr, "invalid WAVE id in wav header\n");
        return -1;
    }

    // read all header chunks. skip unknown chunks.
    found_data = found_fmt = 0;
    while(!found_data) {
        id = read4le(pf);
        chunksize = read4le(pf);
        switch(id) {
            case FMT__ID:
                if(chunksize < 16) {
                    fprintf(stderr, "invalid fmt chunk in wav header\n");
                    return -1;
                }
                pf->wav_format = read2le(pf);
                if(pf->wav_format == WAVE_FORMAT_IEEEFLOAT) {
                    pf->sample_type = PCM_SAMPLE_TYPE_FLOAT;
                } else {
                    pf->sample_type = PCM_SAMPLE_TYPE_INT;
                }
                pf->channels = read2le(pf);
                if(pf->channels == 0) {
                    fprintf(stderr, "invalid number of channels in wav header\n");
                    return -1;
                }
                pf->sample_rate = read4le(pf);
                if(pf->sample_rate == 0) {
                    fprintf(stderr, "invalid sample rate in wav header\n");
                    return -1;
                }
                pf->wav_bps = read4le(pf);
                pf->block_align = read2le(pf);
                pf->bit_width = read2le(pf);
                if(pf->bit_width == 0) {
                    fprintf(stderr, "invalid sample bit width in wav header\n");
                    return -1;
                }
                chunksize -= 16;

                // WAVE_FORMAT_EXTENSIBLE data
                pf->ch_mask = 0;
                if(pf->wav_format == WAVE_FORMAT_EXTENSIBLE && chunksize >= 10) {
                    read4le(pf);    // skip CbSize and ValidBitsPerSample
                    pf->ch_mask = read4le(pf);
                    pf->wav_format = read2le(pf);
                    if(pf->wav_format == WAVE_FORMAT_IEEEFLOAT) {
                        pf->sample_type = PCM_SAMPLE_TYPE_FLOAT;
                    } else {
                        pf->sample_type = PCM_SAMPLE_TYPE_INT;
                    }
                    chunksize -= 10;
                }

                // override block alignment in header
                if(pf->wav_format == WAVE_FORMAT_IEEEFLOAT ||
                        pf->wav_format == WAVE_FORMAT_PCM) {
                    pf->block_align = MAX(1, ((pf->bit_width + 7) >> 3) * pf->channels);
                }

                // make up channel mask if not using WAVE_FORMAT_EXTENSIBLE
                // or if ch_mask is set to zero (unspecified configuration)
                // TODO: select default configurations for >6 channels
                if(pf->ch_mask == 0) {
                    switch(pf->channels) {
                        case 1: pf->ch_mask = 0x04;  break;
                        case 2: pf->ch_mask = 0x03;  break;
                        case 3: pf->ch_mask = 0x07;  break;
                        case 4: pf->ch_mask = 0x107; break;
                        case 5: pf->ch_mask = 0x37;  break;
                        case 6: pf->ch_mask = 0x3F;  break;
                    }
                }

                // skip any leftover bytes in fmt chunk
                if(pcmfile_seek_set(pf, pf->filepos + chunksize)) {
                    fprintf(stderr, "error seeking in wav file\n");
                    return -1;
                }
                found_fmt = 1;
                break;
            case DATA_ID:
                if(!found_fmt) return -1;
                if(chunksize == 0)
                    pf->read_to_eof = 1;
                pf->data_size = chunksize;
                pf->data_start = pf->filepos;
                if(pf->seekable && pf->file_size > 0) {
                    // limit data size to end-of-file
                    if(pf->data_size > 0)
                        pf->data_size = MIN(pf->data_size, pf->file_size - pf->data_start);
                    else
                        pf->data_size = pf->file_size - pf->data_start;
                }
                pf->samples = (pf->data_size / pf->block_align);
                found_data = 1;
                break;
            default:
                // skip unknown chunk
                if(chunksize > 0 && pcmfile_seek_set(pf, pf->filepos + chunksize)) {
                    fprintf(stderr, "error seeking in wav file\n");
                    return -1;
                }
        }
    }

    // set audio data format based on bit depth and sample type
    pf->source_format = PCM_SAMPLE_FMT_UNKNOWN;
    switch(pf->bit_width) {
        case 8:  pf->source_format = PCM_SAMPLE_FMT_U8;  break;
        case 16: pf->source_format = PCM_SAMPLE_FMT_S16; break;
        case 20: pf->source_format = PCM_SAMPLE_FMT_S20; break;
        case 24: pf->source_format = PCM_SAMPLE_FMT_S24; break;
        case 32:
            if(pf->sample_type == PCM_SAMPLE_TYPE_FLOAT)
                pf->source_format = PCM_SAMPLE_FMT_FLT;
            else if(pf->sample_type == PCM_SAMPLE_TYPE_INT)
                pf->source_format = PCM_SAMPLE_FMT_S32;
            break;
        case 64:
            if(pf->sample_type == PCM_SAMPLE_TYPE_FLOAT) {
                pf->source_format = PCM_SAMPLE_FMT_DBL;
            } else {
                fprintf(stderr, "64-bit integer samples not supported\n");
                return -1;
            }
            break;
    }
    pf->order = PCM_BYTE_ORDER_LE;
    pcmfile_set_source_format(pf, pf->source_format);

    return 0;
}
