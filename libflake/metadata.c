/**
 * Flake: FLAC audio encoder
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
 * @file metadata.c
 * FLAC Metadata functions
 */

#include "common.h"

#include "flake.h"
#include "bitio.h"
#include "encode.h"
#include "md5.h"

int
flake_get_streaminfo(const FlakeContext *s, FlakeStreaminfo *strminfo)
{
    FlacEncodeContext *ctx;
    MD5Context md5_bak;

    if(!s || !strminfo)
        return -1;

    if(flake_validate_params(s) < 0)
        return -1;

    ctx = s->private_ctx;

    // get parameters from encoding context
    if(ctx->params.variable_block_size || ctx->params.allow_vbs) {
        strminfo->min_block_size = 16;
    } else {
        strminfo->min_block_size = ctx->params.block_size;
    }
    strminfo->max_block_size    = ctx->params.block_size;
    strminfo->min_frame_size    = 0;
    strminfo->max_frame_size    = ctx->max_frame_size;
    strminfo->sample_rate       = ctx->samplerate;
    strminfo->channels          = ctx->channels;
    strminfo->bits_per_sample   = ctx->bps;
    strminfo->samples           = ctx->sample_count;

    // get MD5 checksum
    md5_bak = ctx->md5ctx;
    md5_final(strminfo->md5sum, &md5_bak);

    return 0;
}

void
flake_write_streaminfo(const FlakeStreaminfo *strminfo, uint8_t *data)
{
    BitWriter bw;
    memset(data, 0, 34);
    bitwriter_init(&bw, data, 34);
    bitwriter_writebits(&bw, 16, strminfo->min_block_size);
    bitwriter_writebits(&bw, 16, strminfo->max_block_size);
    bitwriter_writebits(&bw, 24, strminfo->min_frame_size);
    bitwriter_writebits(&bw, 24, strminfo->max_frame_size);
    bitwriter_writebits(&bw, 20, strminfo->sample_rate);
    bitwriter_writebits(&bw,  3, strminfo->channels-1);
    bitwriter_writebits(&bw,  5, strminfo->bits_per_sample-1);
    bitwriter_writebits(&bw,  4, 0);
    bitwriter_writebits(&bw, 32, strminfo->samples);
    bitwriter_flush(&bw);
    memcpy(&data[18], strminfo->md5sum, 16);
}

static int got_vendor_string = 0;
static char vendor_string[32];

void
flake_init_vorbiscomment(FlakeVorbisComment *vc)
{
    if(!got_vendor_string) {
        const char *version_string = flake_get_version();
        memcpy(vendor_string, "Flake ", 6);
        strcpy(&vendor_string[6], version_string);
    }
    vc->vendor_string = vendor_string;
    vc->num_entries = 0;
    memset(vc->entries, 0, 1024 * sizeof(char *));
}

static int
validate_vorbiscomment_entry(const char *entry)
{
    unsigned int i;
    int valid = 0;
    int end_name = 0;
    for(i=0; i<UINT32_MAX; i++) {
        // check for end of field name
        if(!end_name && entry[i] == '=')
            end_name = 1;
        // check for end of string
        if(entry[i] == '\0') {
            if(end_name)
                valid = 1;
            break;
        }
        // check that field name is within specification
        if(!end_name && (entry[i] < ' ' || entry[i] > '}' || entry[i] == '=')) {
            valid = 0;
            break;
        }
        // TODO: validate UTF-8 in field value
    }
    return !valid;
}

static int
validate_vorbiscomment(const FlakeVorbisComment *vc)
{
    unsigned int i;
    int vendor_ok = 0;
    if(vc->vendor_string == NULL) {
        vendor_ok = 1;
    } else {
        for(i=0; i<UINT32_MAX; i++) {
            if(vc->vendor_string[i] == '\0') {
                vendor_ok = 1;
                break;
            }
        }
    }
    if(!vendor_ok)
        return -1;
    if(vc->num_entries > 1024)
        return -1;
    for(i=0; i<vc->num_entries; i++) {
        if(vc->entries[i] == NULL || validate_vorbiscomment_entry(vc->entries[i]))
            return -1;
    }
    return 0;
}

int
flake_add_vorbiscomment_entry(FlakeVorbisComment *vc, char *entry)
{
    int invalid = validate_vorbiscomment_entry(entry);
    if(!invalid) {
        vc->entries[vc->num_entries++] = entry;
    }
    return invalid;
}

int
flake_get_vorbiscomment_size(const FlakeVorbisComment *vc)
{
    unsigned int i;
    uint64_t size = 0;

    if(validate_vorbiscomment(vc))
        return -1;

    size += 4;
    if(vc->vendor_string != NULL)
        size += strlen(vc->vendor_string);

    size += 4;
    for(i=0; i<vc->num_entries; i++) {
        size += 4 + strlen(vc->entries[i]);
    }

   if(size > INT32_MAX)
       return -1;
   return (int)size;
}

static inline void
write_u32_le(uint8_t *buf, uint32_t val)
{
    buf[0] =  val        & 0xFF;
    buf[1] = (val >>  8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

int
flake_write_vorbiscomment(const FlakeVorbisComment *vc, uint8_t *data)
{
    uint32_t length;
    unsigned int i;
    uint8_t *ptr = data;

   if(flake_get_vorbiscomment_size(vc) < 0)
       return -1;

   // write vendor string
   length = 0;
   if(vc->vendor_string != NULL)
       length = strlen(vc->vendor_string);
   write_u32_le(ptr, length);
   ptr += 4;
   memcpy(ptr, vc->vendor_string, length);
   ptr += length;

   // write number of entries
   write_u32_le(ptr, vc->num_entries);
   ptr += 4;

   // write entries
   for(i=0; i<vc->num_entries; i++) {
       length = strlen(vc->entries[i]);
       write_u32_le(ptr, length);
       ptr += 4;
       memcpy(ptr, vc->entries[i], length);
       ptr += length;
   }

   return 0;
}
