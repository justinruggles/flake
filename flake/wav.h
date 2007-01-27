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

/**
 * @file wav.h
 * WAV decoder header
 */

#ifndef WAV_H
#define WAV_H

#include "common.h"

#define WAVE_FORMAT_PCM         0x0001
#define WAVE_FORMAT_IEEEFLOAT   0x0003
#define WAVE_FORMAT_EXTENSIBLE  0xFFFE

#define WAV_SEEK_SET 0
#define WAV_SEEK_CUR 1
#define WAV_SEEK_END 2

enum WavSampleFormat {
    WAV_SAMPLE_FMT_UNKNOWN = -1,
    WAV_SAMPLE_FMT_U8 = 0,
    WAV_SAMPLE_FMT_S16,
    WAV_SAMPLE_FMT_S20,
    WAV_SAMPLE_FMT_S24,
    WAV_SAMPLE_FMT_S32,
    WAV_SAMPLE_FMT_FLT,
    WAV_SAMPLE_FMT_DBL,
};

typedef struct WavFile {
    FILE *fp;
    uint32_t filepos;
    int seekable;
    uint32_t file_size;
    uint32_t data_start;
    uint32_t data_size;
    uint32_t samples;
    int format;
    int channels;
    uint32_t ch_mask;
    int sample_rate;
    int bytes_per_sec;
    int block_align;
    int bit_width;
    enum WavSampleFormat source_format; // set by wavfile_init
    enum WavSampleFormat read_format;   // set by user
} WavFile;

extern int wavfile_init(WavFile *wf, FILE *fp);

extern int wavfile_read_samples(WavFile *wf, void *buffer, int num_samples);

extern int wavfile_seek_samples(WavFile *wf, int32_t offset, int whence);

extern int wavfile_seek_time_ms(WavFile *wf, int32_t offset, int whence);

extern int wavfile_position(WavFile *wf);

extern void wavfile_print(FILE *st, WavFile *wf);

#endif /* WAV_H */
