/**
 *    x-*-x
 *  x¤\_|_/¤x
 *  *-+«ø»+-*
 *  x¤/¯|¯\¤x
 *    x-*-x
 *
 * Flake: FLAC audio encoder
 * Copyright (c) 2006 Justin Ruggles
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#if (defined __MINGW32__) || (defined _WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include "bswap.h"
#include "wav.h"
#include "flake.h"

static void
print_usage(FILE *out)
{
    fprintf(out, "usage: flake [options] <input.wav> <output.flac>\n"
                 "type 'flake -h' for more details.\n\n");
}

static void
print_help(FILE *out)
{
    fprintf(out, "usage: flake [options] <input.wav> <output.flac>\n"
                 "options:\n"
                 "       [-h]         Print out list of commandline options\n"
                 "       [-p #]       Padding bytes to put in header (default: 4096)\n"
                 "       [-0 ... -12] Compression level (default: 5)\n"
                 "                        0 = -b 1024  -l 0  -o 0 -s 1 -r 2,2\n"
                 "                        1 = -b 1024  -l 4  -o 1 -s 1 -r 2,2\n"
                 "                        2 = -b 1024  -l 4  -o 1 -s 1 -r 0,3\n"
                 "                        3 = -b 2048  -l 6  -o 1 -s 1 -r 0,3\n"
                 "                        4 = -b 4096  -l 8  -o 1 -s 1 -r 0,3\n"
                 "                        5 = -b 4608  -l 8  -o 1 -s 1 -r 0,8\n"
                 "                        6 = -b 4608  -l 8  -o 6 -s 1 -r 0,8\n"
                 "                        7 = -b 4608  -l 8  -o 3 -s 1 -r 0,8\n"
                 "                        8 = -b 4608  -l 12 -o 6 -s 1 -r 0,8\n"
                 "                        9 = -b 4608  -l 12 -o 3 -s 1 -r 0,8\n"
                 "                       10 = -b 4608  -l 12 -o 5 -s 1 -r 0,8\n"
                 "                       11 = -b 4608  -l 32 -o 6 -s 1 -r 0,8\n"
                 "                       12 = -b 4608  -l 32 -o 5 -s 1 -r 0,8\n"
                 "       [-b #]       Block size [16 - 65535] (default: 4608)\n"
                 "       [-l #]       Maximum prediction order [0 - 32] (default: 8)\n"
                 "       [-o #]       Prediction order selection method\n"
                 "                        0 = maximum\n"
                 "                        1 = estimate (default)\n"
                 "                        2 = 2-level\n"
                 "                        3 = 4-level\n"
                 "                        4 = 8-level\n"
                 "                        5 = full search\n"
                 "                        6 = log search\n"
                 "       [-r #[,#]]   Rice partition order {max} or {min},{max} (default: 0,8)\n"
                 "       [-s #]       Stereo decorrelation method\n"
                 "                        0 = independent L+R channels\n"
                 "                        1 = mid-side (default)\n"
                 "\n");
}

typedef struct CommandOptions {
    char *infile;
    char *outfile;
    int found_input;
    int found_output;
    int compr;
    int omethod;
    int omax;
    int pomin;
    int pomax;
    int bsize;
    int stmethod;
    int padding;
} CommandOptions;

static int
parse_files(CommandOptions *opts, char *arg)
{
    if(!opts->found_input) {
        opts->infile = arg;
        opts->found_input = 1;
    } else {
        if(opts->found_output) return 1;
        opts->outfile = arg;
        opts->found_output = 1;
    }
    return 0;
}

static int
parse_number(char *arg, int max) {
    int i;
    int m = 0;
    int n = 0;
    int digits;
    for(i=0; i<max; i++) {
        if(arg[i] == '\0') break;
        if(m == 0) m = 1;
        else m *= 10;
    }
    digits = i;
    for(i=0; i<digits; i++) {
        if(arg[i] < '0' || arg[i] > '9') {
            fprintf(stderr, "invalid digit: %c (ASCII:0x%02X)\n", arg[i], arg[i]);
            return -1;
        }
        n += (arg[i]-48) * m;
        m /= 10;
    }
    return n;
}

static int
parse_commandline(int argc, char **argv, CommandOptions *opts)
{
    int i;
    static const char *param_str = "hbloprs";

    if(argc < 2) {
        return 1;
    }

    opts->infile = argv[1];
    opts->outfile = argv[2];
    opts->found_input = 0;
    opts->found_output = 0;
    opts->compr = 5;
    opts->omethod = -1;
    opts->omax = -1;
    opts->pomin = -1;
    opts->pomax = -1;
    opts->bsize = 0;
    opts->stmethod = -1;
    opts->padding = -1;

    for(i=1; i<argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] != '\0') {
            if(argv[i][1] >= '0' && argv[i][1] <= '9') {
                if(argv[i][2] != '\0' && argv[i][3] != '\0') {
                    if(parse_files(opts, argv[i])) {
                        fprintf(stderr, "error parsing filenames.\n");
                        return 1;
                    }
                } else {
                    opts->compr = parse_number(&argv[i][1], 2);
                    if(opts->compr < 0 || opts->compr > 12) {
                        fprintf(stderr, "invalid compression: %d. must be 0 to 12.\n", opts->compr);
                        return 1;
                    }
                }
            } else {
                // if argument starts with '-' and is more than 1 char, treat
                // it as a filename
                if(argv[i][2] != '\0') {
                    if(parse_files(opts, argv[i])) {
                        fprintf(stderr, "error parsing filenames.\n");
                        return 1;
                    } else {
                        continue;
                    }
                }
                // check to see if param is valid
                if(strchr(param_str, argv[i][1]) == NULL) {
                    fprintf(stderr, "invalid option: -%c\n", argv[i][1]);
                    return 1;
                }
                // print commandline help
                if(argv[i][1] == 'h') {
                    return 2;
                }
                i++;
                if(i >= argc) {
                    fprintf(stderr, "incomplete option: -%c\n", argv[i-1][1]);
                    return 1;
                }

                switch(argv[i-1][1]) {
                    case 'b':
                        opts->bsize = parse_number(argv[i], 5);
                        if(opts->bsize < 16 || opts->bsize > 65535) {
                            fprintf(stderr, "invalid blocksize: %d. must be 16 to 65535.\n", opts->bsize);
                            return 1;
                        }
                        break;
                    case 'l':
                        opts->omax = parse_number(argv[i], 2);
                        if(opts->omax < 0 || opts->omax > 32) {
                            fprintf(stderr, "invalid maximum order: %d. must be 0 to 32.\n", opts->omax);
                            return 1;
                        }
                        break;
                    case 'o':
                        opts->omethod = parse_number(argv[i], 1);
                        if(opts->omethod < 0 || opts->omethod > 6) {
                            fprintf(stderr, "invalid order selection method: %d. must be 0 to 4.\n", opts->omethod);
                            return 1;
                        }
                        break;
                    case 'p':
                        opts->padding = parse_number(argv[i], 8);
                        if(opts->padding < 0 || opts->padding >= (1<<24)) {
                            fprintf(stderr, "invalid order padding amount: %d. must be 0 to 16777215.\n", opts->omethod);
                            return 1;
                        }
                        break;
                    case 'r':
                        if(strchr(argv[i], ',') == NULL) {
                            opts->pomin = 0;
                            opts->pomax = parse_number(argv[i], 1);
                        } else {
                            char *po = strchr(argv[i], ',');
                            po[0] = '\0';
                            opts->pomin = parse_number(argv[i], 1);
                            opts->pomax = parse_number(&po[1], 1);
                        }
                        if(opts->pomin < 0 || opts->pomin > 8) {
                            fprintf(stderr, "invalid min partition order: %d. must be 0 to 8.\n", opts->pomin);
                            return 1;
                        }
                        if(opts->pomax < 0 || opts->pomax > 8) {
                            fprintf(stderr, "invalid max partition order: %d. must be 0 to 8.\n", opts->pomax);
                            return 1;
                        }
                        if(opts->pomin > opts->pomax) {
                            fprintf(stderr, "invalid min partition order: %d. must be <= max partition order.\n", opts->pomin);
                            return 1;
                        }
                        break;
                    case 's':
                        opts->stmethod = parse_number(argv[i], 1);
                        if(opts->stmethod < 0 || opts->stmethod > 1) {
                            fprintf(stderr, "invalid stereo decorrelation method: %d. must be 0 or 1.\n", opts->stmethod);
                            return 1;
                        }
                        break;
                }
            }
        } else {
            // if the argument is a single '-' treat it as a filename
            if(parse_files(opts, argv[i])) {
                fprintf(stderr, "error parsing filenames.\n");
                return 1;
            }
        }
    }
    if(!opts->found_input || !opts->found_output) {
        fprintf(stderr, "error parsing filenames.\n");
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    CommandOptions opts;
    FILE *ifp;
    FILE *ofp;
    WavFile *wf;
    FlakeContext *s;
    int header_size;
    uint8_t *frame;
    int16_t *wav;
    int err, percent;
    uint32_t nr, fs, samplecount, bytecount, framecount;
    int t0, t1;
    float kb, sec, kbps, wav_bytes;
    char *omethod_s, *stmethod_s;

    fprintf(stderr, "\nFlake: FLAC audio encoder\n(c) 2006  Justin Ruggles\n\n");

    err = parse_commandline(argc, argv, &opts);
    if(err) {
        if(err == 2) {
            print_help(stdout);
            return 0;
        } else {
            print_usage(stderr);
            return 1;
        }
    }

    if(!strncmp(opts.infile, "-", 2)) {
#if defined(__MINGW32__)
        setmode(fileno(stdin), O_BINARY);
#elif defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        ifp = stdin;
    } else {
        ifp = fopen(opts.infile, "rb");
        if(!ifp) {
            fprintf(stderr, "error opening input file: %s\n", opts.infile);
            return 1;
        }
    }
    if(!strncmp(opts.outfile, "-", 2)) {
#if defined(__MINGW32__)
        setmode(fileno(stdout), O_BINARY);
#elif defined(_WIN32)
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        ofp = stdout;
    } else {
        ofp = fopen(opts.outfile, "wb");
        if(!ofp) {
            fprintf(stderr, "error opening output file: %s\n", opts.outfile);
            return 1;
        }
    }

    wf = malloc(sizeof(WavFile));
    if(wavfile_init(wf, ifp)) {
        fprintf(stderr, "invalid wav file: %s\n", opts.infile);
        free(wf);
        return 1;
    }
    wf->read_format = WAV_SAMPLE_FMT_S16;
    wavfile_print(stderr, wf);
    if(wf->samples > 0) {
        fprintf(stderr, "samples: %d\n", wf->samples);
    } else {
        fprintf(stderr, "samples: unknown\n");
    }

    // initialize encoder
    s = malloc(sizeof(FlakeContext));
    s->channels = wf->channels;
    s->sample_rate = wf->sample_rate;
    s->bits_per_sample = 16;
    if(wf->bit_width != 16) {
        fprintf(stderr, "warning! converting to 16-bit (not lossless)\n");
    }
    s->samples = wf->samples;
    s->block_size = opts.bsize;
    s->compression = opts.compr;
    s->order_method = opts.omethod;
    s->stereo_method = opts.stmethod;
    s->max_order = opts.omax;
    s->min_partition_order = opts.pomin;
    s->max_partition_order = opts.pomax;
    s->padding_size = opts.padding;
    header_size = flake_encode_init(s);
    if(header_size < 0) {
        free(wf);
        if(s->private_ctx) free(s->private_ctx);
        free(s);
        fprintf(stderr, "Error initializing encoder.\n");
        exit(1);
    }
    fwrite(s->header, 1, header_size, ofp);

    // print encoding options info
    fprintf(stderr, "\nblocksize: %d\n", s->block_size);
    fprintf(stderr, "max prediction order: %d\n", s->max_order);
    fprintf(stderr, "partition order: %d,%d\n", s->min_partition_order, s->max_partition_order);
    omethod_s = "ERROR";
    switch(s->order_method) {
        case 0: omethod_s = "maximum";  break;
        case 1: omethod_s = "estimate"; break;
        case 2: omethod_s = "2-level"; break;
        case 3: omethod_s = "4-level"; break;
        case 4: omethod_s = "8-level"; break;
        case 5: omethod_s = "full search";   break;
        case 6: omethod_s = "log search";  break;
    }
    fprintf(stderr, "order method: %s\n", omethod_s);
    if(s->channels == 2) {
        stmethod_s = "ERROR";
        switch(s->stereo_method) {
            case 0: stmethod_s = "independent";  break;
            case 1: stmethod_s = "mid-side";     break;
        }
        fprintf(stderr, "stereo method: %s\n\n", stmethod_s);
    }

    frame = malloc(s->max_frame_size);
    wav = malloc(s->block_size * wf->channels * sizeof(int16_t));

    samplecount = framecount = t0 = percent = 0;
    wav_bytes = 0;
    bytecount = header_size;
    nr = wavfile_read_samples(wf, wav, s->block_size);
    while(nr > 0) {
        s->block_size = nr;
        fs = flake_encode_frame(s, frame, wav);
        if(fs < 0) {
            fprintf(stderr, "Error encoding frame\n");
        } else if(fs > 0) {
            fwrite(frame, 1, fs, ofp);
            samplecount += s->block_size;
            bytecount += fs;
            framecount++;
            t1 = samplecount / s->sample_rate;
            if(t1 > t0) {
                kb = ((bytecount * 8.0) / 1000.0);
                sec = ((float)samplecount) / ((float)s->sample_rate);
                if(samplecount > 0) kbps = kb / sec;
                else kbps = kb;
                if(s->samples > 0) {
                    percent = ((samplecount * 100.5) / s->samples);
                }
                wav_bytes = samplecount*wf->block_align;
                fprintf(stderr, "\rprogress: %3d%% | ratio: %1.3f | "
                                "bitrate: %4.1f kbps ",
                        percent, (bytecount / wav_bytes), kbps);
            }
            t0 = t1;
        }
        nr = wavfile_read_samples(wf, wav, s->block_size);
    }
    fprintf(stderr, "| bytes: %d \n\n", bytecount);

    flake_encode_close(s);

    // if seeking is possible, rewrite sample count and MD5 checksum
    if(!fseek(ofp, 22, SEEK_SET)) {
        uint32_t sc = be2me_32(samplecount);
        fwrite(&sc, 4, 1, ofp);
        fwrite(s->md5digest, 1, 16, ofp);
    }

    free(wav);
    free(frame);
    free(s);
    free(wf);

    fclose(ifp);
    fclose(ofp);

    return 0;
}
