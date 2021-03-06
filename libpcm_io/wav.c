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

static int
wave_probe(uint8_t *data, int size)
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

static int
wave_init(PcmFile *pf)
{
    int id, found_data, found_fmt, chunksize;
    int tag=1, channels=2, sample_rate=44100, block_align=4, bits=16;
    enum PcmSampleFormat fmt = PCM_SAMPLE_FMT_S16;

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
                tag = read2le(pf);
                channels = read2le(pf);
                sample_rate = read4le(pf);
                pf->wav_bps = read4le(pf);
                block_align = read2le(pf);
                bits = read2le(pf);
                chunksize -= 16;

                // WAVE_FORMAT_EXTENSIBLE data
                pf->ch_mask = 0;
                if(tag == WAVE_FORMAT_EXTENSIBLE && chunksize >= 10) {
                    read4le(pf);    // skip CbSize and ValidBitsPerSample
                    pf->ch_mask = read4le(pf);
                    tag = read2le(pf);
                    chunksize -= 10;
                }

                // check header params
                if(tag != WAVE_FORMAT_PCM) {
                    fprintf(stderr, "unsupported wFormatTag: 0x%02X\n", tag);
                    return -1;
                }
                if(channels == 0) {
                    fprintf(stderr, "invalid number of channels in wav header\n");
                    return -1;
                }
                if(sample_rate == 0) {
                    fprintf(stderr, "invalid sample rate in wav header\n");
                    return -1;
                }
                if(bits == 0) {
                    fprintf(stderr, "invalid sample bit width in wav header\n");
                    return -1;
                }

                // override block alignment in header
                block_align = MAX(1, ((bits + 7) >> 3) * channels);

                // use default channel mask if necessary
                if(pf->ch_mask == 0) {
                    pf->ch_mask = pcmfile_get_default_ch_mask(channels);
                }

                // skip any leftover bytes in fmt chunk
                chunksize += chunksize & 1;
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
                pf->samples = (pf->data_size / block_align);
                found_data = 1;
                break;
            default:
                // skip unknown chunk
                chunksize += chunksize & 1;
                if(chunksize > 0 && pcmfile_seek_set(pf, pf->filepos + chunksize)) {
                    fprintf(stderr, "error seeking in wav file\n");
                    return -1;
                }
        }
    }

    // set audio data format based on bit depth and sample type
    fmt = PCM_SAMPLE_FMT_UNKNOWN;
    switch(bits) {
        case 8:  fmt = PCM_SAMPLE_FMT_U8;  break;
        case 16: fmt = PCM_SAMPLE_FMT_S16; break;
        case 20: fmt = PCM_SAMPLE_FMT_S20; break;
        case 24: fmt = PCM_SAMPLE_FMT_S24; break;
        case 32:
                fmt = PCM_SAMPLE_FMT_S32;
            break;
        default:
            fprintf(stderr, "unsupported bit depth: %d\n", bits);
            return -1;
    }
    pf->internal_fmt = tag;
    pcmfile_set_source_params(pf, channels, fmt, PCM_BYTE_ORDER_LE, sample_rate);

    return 0;
}

PcmFormat wave_format = {
    "wave",
    "Microsoft WAVE",
    PCM_FORMAT_WAVE,
    wave_probe,
    wave_init,
    NULL
};
