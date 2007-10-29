/**
 * Stripped-down API example for libFlake
 * Simple 16-bit WAVE to FLAC encoder without any commandline options
 *
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
 * How to compile this example program:
 * This assumes the default install targets.
 * gcc -o api_example api_example.c -lflake -lm
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <flake/flake.h>

FILE *input_file = NULL;
FILE *output_file = NULL;

/** Generic commandline processing and file i/o */
static int
open_files(int argc, char **argv)
{
    if(argc != 3) {
        fprintf(stderr, "invalid input parameters\n");
        if(argc > 0)
            fprintf(stderr, "usage: %s input.wav output.flac\n", argv[0]);
        return -1;
    }

    input_file = fopen(argv[1], "rb");
    if(!input_file) {
        fprintf(stderr, "invalid input file: %s\n", argv[1]);
        return -1;
    }

    output_file = fopen(argv[2], "wb");
    if(!output_file) {
        fprintf(stderr, "invalid output file: %s\n", argv[2]);
        return -1;
    }

    return 0;
}

static void
close_files()
{
    if(input_file)
        fclose(input_file);
    if(output_file)
        fclose(output_file);
}

/** WAVE reader helper function. Read 32-bit little-endian integer. */
static inline uint32_t
read4le(FILE *fp)
{
    uint32_t x;
    x  = fgetc(input_file);
    x |= fgetc(input_file) << 8;
    x |= fgetc(input_file) << 16;
    x |= fgetc(input_file) << 24;
    return x;
}

/** WAVE reader helper function. Read 16-bit little-endian integer. */
static inline uint16_t
read2le(FILE *fp)
{
    uint16_t x;
    x = fgetc(input_file);
    x |= fgetc(input_file) << 8;
    return x;
}

/** Generic WAVE header parsing */
static int
parse_wav_header(FlakeContext *s)
{
    int id, chunksize, found_fmt, found_data, wformattag;

    if((id=read4le(input_file)) != 0x46464952)
        return -1;
    read4le(input_file);
    if(read4le(input_file) != 0x45564157)
        return -1;
    found_data = found_fmt = 0;
    while(!found_data && !feof(input_file) && !ferror(input_file)) {
        id = read4le(input_file);
        chunksize = read4le(input_file);
        switch(id) {
            case 0x20746D66:
                if(chunksize < 16)
                    return -1;
                wformattag = read2le(input_file);
                s->channels = read2le(input_file);
                s->sample_rate = read4le(input_file);
                read4le(input_file);
                read2le(input_file);
                s->bits_per_sample = read2le(input_file);
                chunksize -= 16;
                if(wformattag == 0xFFFE && chunksize >= 10) {
                    read4le(input_file);
                    read4le(input_file);
                    wformattag = read2le(input_file);
                    chunksize -= 10;
                }
                if(wformattag != 1 || s->bits_per_sample != 16)
                    return -1;
                while(chunksize-- > 0 && !feof(input_file) && !ferror(input_file))
                    fgetc(input_file);
                found_fmt = 1;
                break;
            case 0x61746164:
                if(!found_fmt)
                    return -1;
                s->samples = (chunksize / (2 * s->channels));
                found_data = 1;
                break;
            default:
                while(chunksize-- > 0 && !feof(input_file) && !ferror(input_file))
                    fgetc(input_file);
        }
    }
    if(!found_data)
        return -1;
    return 0;
}

/** Generic function to read 16-bit raw PCM */
static int
read_samples(FlakeContext *s, int16_t *samples, int num_samples)
{
    int i, j, k;

    k = 0;
    for(i=0; i<num_samples; i++) {
        for(j=0; j<s->channels; j++) {
            samples[k++] = (int16_t)read2le(input_file);
        }
        if(feof(input_file) || ferror(input_file))
            break;
    }
    return i;
}

int
main(int argc, char **argv)
{
    FlakeContext s;             ///< encoding context
    int header_size;            ///< size, in bytes, of FLAC header
    uint8_t *frame_buffer;      ///< pointer to libFlake internal buffer
    int16_t *input_samples;     ///< array of input audio samples
    uint32_t sample_count;      ///< running count of samples encoded
    int num_samples;            ///< number of samples read each cycle (block)
    int frame_bytes;            ///< size, in bytes, of each encoded frame
    float percent_complete;     ///< running completion percentage

    // setup I/O
    if(open_files(argc, argv))
        return 1;

    // read audio info from WAVE header
    if(parse_wav_header(&s)) {
        fprintf(stderr, "error opening WAVE file\n");
        return 1;
    }

    // set compression level and initialize parameters
    s.params.compression = 8;
    if(flake_set_defaults(&s.params)) {
        fprintf(stderr, "Error setting compression level defaults\n");
        return 1;
    }

    // initialize encoder
    header_size = flake_encode_init(&s);
    if(header_size < 0) {
        fprintf(stderr, "Error initializing encoder\n");
        flake_encode_close(&s);
        return 1;
    }

    // write FLAC file header to output
    fwrite(s.header, 1, header_size, output_file);

    // get pointer to libFlake's internal frame buffer
    frame_buffer = flake_get_buffer(&s);

    // initialize audio input buffer
    input_samples = malloc(s.params.block_size * s.channels * sizeof(int16_t));

    // main encoding loop
    sample_count = 0;
    percent_complete = 0.0f;
    num_samples = read_samples(&s, input_samples, s.params.block_size);
    while(num_samples > 0) {
        frame_bytes = flake_encode_frame(&s, input_samples, num_samples);
        if(frame_bytes < 0) {
            fprintf(stderr, "\nError encoding frame\n");
        } else if(frame_bytes > 0) {
            fwrite(frame_buffer, 1, frame_bytes, output_file);
            sample_count += num_samples;
            if(s.samples > 0)
                percent_complete = ((sample_count * 100.5f) / s.samples);
            fprintf(stderr, "\rprogress: %3d%% ", (int)percent_complete);
        }
        num_samples = read_samples(&s, input_samples, s.params.block_size);
    }
    fprintf(stderr, "\n");

    // if seeking is possible, rewrite MD5 checksum in header
    if(!fseek(files->ofp, 8, SEEK_SET)) {
        FlakeStreaminfo strminfo;
        if(!flake_metadata_get_streaminfo(&s, &strminfo)) {
            uint8_t strminfo_data[34];
            flake_metadata_write_streaminfo(&strminfo, strminfo_data);
            fwrite(strminfo_data, 1, 34, files->ofp);
        }
    }

    // close encoder, free input buffer, close files, and exit
    flake_encode_close(&s);
    free(input_samples);
    close_files();
    return 0;
}

