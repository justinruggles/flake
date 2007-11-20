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
 * @file pcm_io.c
 * raw PCM decoder
 */

/**
 * HAVE_CONFIG_H needs to be defined and
 * WORDS_BIGENDIAN needs to be defined in config.h if the system uses
 * big-endian words (e.g. PowerPC)
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "bswap.h"
#include "pcm_io_common.h"
#include "pcm_io.h"

int
pcmfile_seek_set(PcmFile *pf, uint64_t dest)
{
    FILE *fp = pf->io.fp;
    int slow_seek = !(pf->seekable);

    if(pf->seekable) {
        if(dest <= INT32_MAX) {
            // destination is within first 2GB
            if(fseek(fp, (long)dest, SEEK_SET)) return -1;
        } else {
            int64_t offset = (int64_t)dest - (int64_t)pf->filepos;
            if(offset >= INT32_MIN && offset <= INT32_MAX) {
                // offset is within +/- 2GB of file start
                if(fseek(fp, (long)offset, SEEK_CUR)) return -1;
            } else {
                // absolute offset is more than 2GB
                if(offset < 0) {
                    fprintf(stderr, "error: backward seeking is limited to 2GB\n");
                    return -1;
                } else {
                    fprintf(stderr, "warning: forward seeking more than 2GB will be slow.\n");
                }
                slow_seek = 1;
            }
        }
        byteio_flush(&pf->io);
    }
    if(slow_seek) {
        // do forward-only seek by reading data to temp buffer
        uint64_t offset;
        uint8_t buf[1024];

        if(dest < pf->filepos)
            return -1;

        for(offset = dest - pf->filepos; offset > 1024; offset -= 1024)
            byteio_read(buf, 1024, &pf->io);

        byteio_read(buf, offset, &pf->io);
    }
    pf->filepos = dest;

    return 0;
}

int
pcmfile_init(PcmFile *pf, FILE *fp, enum PcmSampleFormat read_format,
             int file_format)
{
    if(pf == NULL || fp == NULL) {
        fprintf(stderr, "null input to pcmfile_init()\n");
        return -1;
    }

    memset(pf, 0, sizeof(PcmFile));
    pf->file_format = file_format;
    pf->read_format = read_format;

    // attempt to get file size
#ifdef _WIN32
    // in Windows, don't try to detect seeking support for stdin
    if(fp != stdin) {
        pf->seekable = !fseek(fp, 0, SEEK_END);
    }
#else
    pf->seekable = !fseek(fp, 0, SEEK_END);
#endif
    if(pf->seekable) {
        // TODO: portable 64-bit ftell
        long fs = ftell(fp);
        // ftell should return an error if value cannot fit in return type
        if(fs < 0) {
            fprintf(stderr, "Warning, unsupported file size.\n");
            pf->file_size = 0;
        } else {
            pf->file_size = (uint64_t)fs;
        }
        fseek(fp, 0, SEEK_SET);
    }
    pf->filepos = 0;
    if(byteio_init(&pf->io, fp)) {
        fprintf(stderr, "error initializing byte buffer\n");
        return -1;
    }

    // detect file format if not specified by the user
    pcmfile_register_all_formats();
    if(pf->file_format == PCM_FORMAT_UNKNOWN) {
        uint8_t probe_data[12];
        byteio_peek(probe_data, 12, &pf->io);
        pf->pcm_format = pcmfile_probe_format(probe_data, 12);
        pf->file_format = pf->pcm_format->format;
    } else {
        pf->pcm_format = pcmfile_find_format(pf->file_format);
    }
    if(pf->pcm_format == NULL) {
        fprintf(stderr, "unable to detect file format\n");
        return -1;
    }

    // initialize format
    if(pf->pcm_format->init && pf->pcm_format->init(pf))
        return -1;

    return 0;
}

void
pcmfile_close(PcmFile *pf)
{
    byteio_close(&pf->io);
}

int
pcmfile_read_samples(PcmFile *pf, void *output, int num_samples)
{
    uint8_t *buffer;
    uint8_t *read_buffer;
    uint32_t bytes_needed, buffer_size;
    int nr, i, j, bps, nsmp;

    // check input and limit number of samples
    if(pf == NULL || pf->io.fp == NULL || output == NULL || pf->fmt_convert == NULL) {
        fprintf(stderr, "null input to pcmfile_read_samples()\n");
        return -1;
    }
    if(pf->block_align <= 0) {
        fprintf(stderr, "invalid block_align\n");
        return -1;
    }
    num_samples = MIN(num_samples, PCM_MAX_READ);

    // calculate number of bytes to read, being careful not to read past
    // the end of the data chunk
    bytes_needed = pf->block_align * num_samples;
    if(!pf->read_to_eof) {
        if((pf->filepos + bytes_needed) >= (pf->data_start + pf->data_size)) {
            bytes_needed = (uint32_t)((pf->data_start + pf->data_size) - pf->filepos);
            num_samples = bytes_needed / pf->block_align;
        }
    }
    if(num_samples <= 0) return 0;

    // allocate temporary buffer for raw input data
    bps = pf->block_align / pf->channels;
    buffer_size = (bps != 3) ? bytes_needed : num_samples * sizeof(int32_t) * pf->channels;
    buffer = calloc(buffer_size+1, 1);
    if(!buffer) {
        fprintf(stderr, "error allocating read buffer\n");
        return -1;
    }
    read_buffer = buffer + (buffer_size - bytes_needed);

    // read raw audio samples from input stream into temporary buffer
    nr = byteio_read(read_buffer, bytes_needed, &pf->io);
    if (nr <= 0) {
        free(buffer);
        return nr;
    }
    pf->filepos += nr;
    nr /= pf->block_align;
    nsmp = nr * pf->channels;

    // do any necessary conversion based on source_format and read_format.
    // also do byte swapping when necessary based on source audio and system
    // byte orders.
    switch (bps) {
    case 2:
#ifdef WORDS_BIGENDIAN
        if(pf->order == PCM_BYTE_ORDER_LE)
#else
        if(pf->order == PCM_BYTE_ORDER_BE)
#endif
        {
            uint16_t *buf16 = (uint16_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf16[i] = bswap_16(buf16[i]);
            }
        }
        break;
    case 3:
        {
            int32_t *input = (int32_t*)buffer;
            int unused_bits = 32 - pf->bit_width;
            int32_t v;
            for(i=0,j=0; i<nsmp*bps; i+=bps,j++) {
#ifdef WORDS_BIGENDIAN
                if(pf->order == PCM_BYTE_ORDER_LE)
#else
                if(pf->order == PCM_BYTE_ORDER_BE)
#endif
                {
                    v = be2me_32((*(int32_t*)(read_buffer + i)) << 8);
                } else {
                    v = *(int32_t*)(read_buffer + i);
                }
                v <<= unused_bits; // clear unused high bits
                v >>= unused_bits; // sign extend
                input[j] = v;
            }
        }
        break;
    case 4:
#ifdef WORDS_BIGENDIAN
        if(pf->order == PCM_BYTE_ORDER_LE)
#else
        if(pf->order == PCM_BYTE_ORDER_BE)
#endif
        {
            uint32_t *buf32 = (uint32_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf32[i] = bswap_32(buf32[i]);
            }
        }
        break;
    default:
#ifdef WORDS_BIGENDIAN
        if(pf->order == PCM_BYTE_ORDER_LE)
#else
        if(pf->order == PCM_BYTE_ORDER_BE)
#endif
        {
            uint64_t *buf64 = (uint64_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf64[i] = bswap_64(buf64[i]);
            }
        }
        break;
    }
    pf->fmt_convert(output, buffer, nsmp);

    // free temporary buffer
    free(buffer);

    return nr;
}

int
pcmfile_seek_samples(PcmFile *pf, int64_t offset, int whence)
{
    int64_t byte_offset;
    uint64_t newpos, fpos, dst, dsz;

    if(pf == NULL || pf->io.fp == NULL) return -1;
    if(pf->block_align <= 0) return -1;
    if(pf->filepos < pf->data_start) return -1;
    if(pf->data_size == 0) return 0;

    fpos = pf->filepos;
    dst = pf->data_start;
    dsz = pf->data_size;
    byte_offset = offset;
    byte_offset *= pf->block_align;

    // calculate new destination within file
    switch(whence) {
        case PCM_SEEK_SET:
            newpos = dst + CLIP(byte_offset, 0, (int64_t)dsz);
            break;
        case PCM_SEEK_CUR:
            newpos = fpos - MIN(-byte_offset, (int64_t)(fpos - dst));
            newpos = MIN(newpos, dst + dsz);
            break;
        case PCM_SEEK_END:
            newpos = dst + dsz - CLIP(byte_offset, 0, (int64_t)dsz);
            break;
        default: return -1;
    }

    // seek to the destination point
    if(pcmfile_seek_set(pf, newpos)) return -1;

    return 0;
}

int
pcmfile_seek_time_ms(PcmFile *pf, int64_t offset, int whence)
{
    int64_t samples;
    if(pf == NULL) return -1;
    samples = offset * pf->sample_rate / 1000;
    return pcmfile_seek_samples(pf, samples, whence);
}

uint64_t
pcmfile_position(PcmFile *pf)
{
    uint64_t cur;

    if(pf == NULL) return -1;
    if(pf->block_align <= 0) return -1;
    if(pf->data_start == 0 || pf->data_size == 0) return 0;

    cur = (pf->filepos - pf->data_start) / pf->block_align;
    return cur;
}

uint64_t
pcmfile_position_time_ms(PcmFile *pf)
{
    return (pcmfile_position(pf) * 1000 / pf->sample_rate);
}

void
pcmfile_print(PcmFile *pf, FILE *st)
{
    const char *type, *chan, *fmt, *order;
    if(st == NULL || pf == NULL) return;
    type = "?";
    chan = "?-channel";
    fmt = "unknown";
    order = "?-endian";
    if(pf->sample_type == PCM_SAMPLE_TYPE_INT) {
        if(pf->bit_width > 8) type = "Signed";
        else type = "Unsigned";
    } else if(pf->sample_type == PCM_SAMPLE_TYPE_FLOAT) {
        type = "Floating-point";
    } else {
        type = "[unsupported type]";
    }
    if(pf->ch_mask & 0x08) {
        switch(pf->channels-1) {
            case 1: chan = "1.1-channel"; break;
            case 2: chan = "2.1-channel"; break;
            case 3: chan = "3.1-channel"; break;
            case 4: chan = "4.1-channel"; break;
            case 5: chan = "5.1-channel"; break;
            default: chan = "multi-channel with LFE"; break;
        }
    } else {
        switch(pf->channels) {
            case 1: chan = "mono"; break;
            case 2: chan = "stereo"; break;
            case 3: chan = "3-channel"; break;
            case 4: chan = "4-channel"; break;
            case 5: chan = "5-channel"; break;
            case 6: chan = "6-channel"; break;
            default: chan = "multi-channel"; break;
        }
    }
    if(pf->pcm_format)
        fmt = pf->pcm_format->long_name;
    switch(pf->order) {
        case PCM_BYTE_ORDER_LE: order = "little-endian"; break;
        case PCM_BYTE_ORDER_BE: order = "big-endian"; break;
    }
    fprintf(st, "%s %s %d-bit %s %d Hz %s\n", fmt, type, pf->bit_width, order,
            pf->sample_rate, chan);
}

int
pcmfile_get_default_ch_mask(int channels)
{
    static const int nch_to_mask[8] = {
        PCM_CHANNEL_LAYOUT_1_0_0,
        PCM_CHANNEL_LAYOUT_2_0_0,
        PCM_CHANNEL_LAYOUT_3_0_0,
        PCM_CHANNEL_LAYOUT_2_2_0,
        PCM_CHANNEL_LAYOUT_3_2_0,
        PCM_CHANNEL_LAYOUT_3_2_1,
        PCM_CHANNEL_LAYOUT_3_3_1,
        PCM_CHANNEL_LAYOUT_3_4_1
    };
    if(channels < 1 || channels > 8)
        return 0;
    return nch_to_mask[channels-1];
}
