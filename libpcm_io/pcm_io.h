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
 * @file pcm_io.h
 * raw PCM decoder header
 */

#ifndef PCM_IO_H
#define PCM_IO_H

#include <stdio.h>
#include <inttypes.h>

#include "byteio.h"

/* "whence" values for seek functions */
#define PCM_SEEK_SET 0
#define PCM_SEEK_CUR 1
#define PCM_SEEK_END 2

/* maximum single read size: 5 sec at 48kHz */
#define PCM_MAX_READ 240000

/* supported TWOCC WAVE formats */
#define WAVE_FORMAT_PCM         0x0001
#define WAVE_FORMAT_EXTENSIBLE  0xFFFE

/* speaker locations for channel mask */
#define PCM_SPEAKER_FRONT_LEFT              0x1
#define PCM_SPEAKER_FRONT_RIGHT             0x2
#define PCM_SPEAKER_FRONT_CENTER            0x4
#define PCM_SPEAKER_LOW_FREQUENCY           0x8
#define PCM_SPEAKER_BACK_LEFT               0x10
#define PCM_SPEAKER_BACK_RIGHT              0x20
#define PCM_SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define PCM_SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define PCM_SPEAKER_BACK_CENTER             0x100
#define PCM_SPEAKER_SIDE_LEFT               0x200
#define PCM_SPEAKER_SIDE_RIGHT              0x400

/* speaker combinations */
#define PCM_SPEAKERS_FRONT        (PCM_SPEAKER_FRONT_LEFT | PCM_SPEAKER_FRONT_RIGHT)
#define PCM_SPEAKERS_BACK         (PCM_SPEAKER_BACK_LEFT  | PCM_SPEAKER_BACK_RIGHT)
#define PCM_SPEAKERS_SURROUND     (PCM_SPEAKER_SIDE_LEFT  | PCM_SPEAKER_SIDE_RIGHT)

/* some common speaker layouts */
#define PCM_CHANNEL_LAYOUT_1_0_0  (PCM_SPEAKER_FRONT_CENTER)
#define PCM_CHANNEL_LAYOUT_2_0_0  (PCM_SPEAKERS_FRONT)
#define PCM_CHANNEL_LAYOUT_3_0_0  (PCM_SPEAKERS_FRONT       | PCM_SPEAKER_FRONT_CENTER)
#define PCM_CHANNEL_LAYOUT_2_2_0  (PCM_SPEAKERS_FRONT       | PCM_SPEAKERS_BACK)
#define PCM_CHANNEL_LAYOUT_3_2_0  (PCM_CHANNEL_LAYOUT_3_0_0 | PCM_SPEAKERS_SURROUND)
#define PCM_CHANNEL_LAYOUT_3_2_1  (PCM_CHANNEL_LAYOUT_3_2_0 | PCM_SPEAKER_LOW_FREQUENCY)
#define PCM_CHANNEL_LAYOUT_3_3_1  (PCM_CHANNEL_LAYOUT_3_2_1 | PCM_SPEAKER_BACK_CENTER)
#define PCM_CHANNEL_LAYOUT_3_4_1  (PCM_CHANNEL_LAYOUT_3_2_1 | PCM_SPEAKERS_BACK)

/* supported raw audio sample formats */
enum PcmSampleFormat {
    PCM_SAMPLE_FMT_UNKNOWN = -1,
    PCM_SAMPLE_FMT_U8 = 0,
    PCM_SAMPLE_FMT_S16,
    PCM_SAMPLE_FMT_S20,
    PCM_SAMPLE_FMT_S24,
    PCM_SAMPLE_FMT_S32,
};

enum PcmDataFormat {
    PCM_DATA_FORMAT_U8,
    PCM_DATA_FORMAT_S16,
    PCM_DATA_FORMAT_S32
};

/* supported file formats */
enum PcmFileFormat {
    PCM_FORMAT_UNKNOWN = -1,
    PCM_FORMAT_RAW     =  0,
    PCM_FORMAT_WAVE    =  1,
    PCM_FORMAT_AIFF    =  2
};

/* byte orders */
enum PcmByteOrder {
    PCM_BYTE_ORDER_LE = 0,
    PCM_BYTE_ORDER_BE = 1
};

struct PcmFormat;

/* main decoder context */
typedef struct PcmFile {
    /** Format conversion function */
    void (*fmt_convert)(void *dest_v, void *src_v, int n);

    ByteIOContext io;       ///< input buffer
    uint64_t filepos;       ///< current file position
    int seekable;           ///< indicates if input stream is seekable
    int read_to_eof;        ///< indicates that data is to be read until EOF
    uint64_t file_size;     ///< total file size, if known
    uint64_t data_start;    ///< byte position for start of data
    uint64_t data_size;     ///< data size, in bytes
    uint64_t samples;       ///< total number of audio samples

    int file_format;        ///< file format (raw, wav, etc...)
    struct PcmFormat *pcm_format;   ///< pointer to PcmFormat struct
    int order;              ///< sample byte order
    int channels;           ///< number of channels
    uint32_t ch_mask;       ///< channel mask, indicates speaker locations
    int sample_rate;        ///< audio sampling frequency
    int block_align;        ///< bytes in each sample, for all channels
    int bit_width;          ///< bits-per-sample

    enum PcmSampleFormat source_format;      ///< sample type in the input file
    enum PcmDataFormat   read_format;        ///< data type to convert to when reading

    int internal_fmt;       ///< internal format (e.g. WAVE wFormatTag)
    int wav_bps;            ///< WAVE bytes-per-second
} PcmFile;

/**
 * Initializes PcmFile structure using the given input file pointer.
 * Examines the header (if present) to get audio information and has the file
 * pointer aligned at start of data when it exits.
 * Returns non-zero value if an error occurs.
 */
extern int pcmfile_init(PcmFile *pf, FILE *fp, enum PcmDataFormat read_format, int file_format);

/**
 * Frees memory from internal buffer.
 */
extern void pcmfile_close(PcmFile *pf);

/**
 * Sets the source sample format
 */
extern void pcmfile_set_source_format(PcmFile *pf, enum PcmSampleFormat fmt);

/**
 * Sets the source audio information
 */
extern void pcmfile_set_source_params(PcmFile *pf, int ch, enum PcmSampleFormat fmt, int order, int sr);

/**
 * Sets the requested read format
 */
extern void pcmfile_set_read_format(PcmFile *pf, enum PcmDataFormat read_format);

/**
 * Reads audio samples to the output buffer.
 * Output is channel-interleaved, native byte order.
 * Only up to PCM_MAX_READ samples can be read in one call.
 * The output sample format depends on the value of pf->read_format.
 * Returns number of samples read or -1 on error.
 */
extern int pcmfile_read_samples(PcmFile *pf, void *buffer, int num_samples);

/**
 * Seeks to byte offset within file.
 * Limits the seek position or offset to signed 32-bit.
 * It also does slower forward seeking for streaming input.
 */
extern int pcmfile_seek_set(PcmFile *pf, uint64_t dest);

/**
 * Seeks to sample offset.
 * Syntax works like fseek. use PCM_SEEK_SET, PCM_SEEK_CUR, or PCM_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int pcmfile_seek_samples(PcmFile *pf, int64_t offset, int whence);

/**
 * Seeks to time offset, in milliseconds, based on the audio sample rate.
 * Syntax works like fseek. use PCM_SEEK_SET, PCM_SEEK_CUR, or PCM_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int pcmfile_seek_time_ms(PcmFile *pf, int64_t offset, int whence);

/**
 * Returns the current stream position, in samples.
 * Returns -1 on error.
 */
extern uint64_t pcmfile_position(PcmFile *pf);

/**
 * Returns the current stream position, in milliseconds.
 * Returns -1 on error.
 */
extern uint64_t pcmfile_position_time_ms(PcmFile *pf);

/**
 * Prints out a description of the pcm format to the specified
 * output stream.
 */
extern void pcmfile_print(PcmFile *pf, FILE *st);

/**
 * Returns a default channel mask value based on the number of channels
 */
extern int pcmfile_get_default_ch_mask(int channels);

/**
 * File format functions
 */

typedef struct PcmFormat {
    const char *name;
    const char *long_name;
    int format;
    int (*probe)(uint8_t *data, int size);
    int (*init)(PcmFile *pf);
    struct PcmFormat *next;
} PcmFormat;

extern void pcmfile_register_all_formats(void);

extern void pcmfile_register_format(PcmFormat *format);

extern PcmFormat *pcmfile_find_format(int format);

extern PcmFormat *pcmfile_probe_format(uint8_t *data, int size);

#endif /* PCM_IO_H */
