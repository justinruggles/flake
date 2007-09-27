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
 * @file common.h
 * Common header file
 */

#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <math.h>
#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#ifndef EMULATE_INTTYPES
#include <inttypes.h>
#else
#if defined(_WIN32) && defined(_MSC_VER)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif
#endif /* EMULATE_INTTYPES */

#define ABS(a) ((a) >= 0 ? (a) : (-(a)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define CLIP(x,min,max) MAX(MIN((x), (max)), (min))

static inline int
log2i(uint32_t v)
{
    int i;
    int n = 0;
    if(v & 0xffff0000){ v >>= 16; n += 16; }
    if(v & 0xff00){ v >>= 8; n += 8; }
    for(i=2; i<256; i<<=1) {
        if(v >= i) n++;
        else break;
    }
    return n;
}

#include <string.h>

// strnlen is a GNU extention. providing implementation if needed.
#ifndef HAVE_STRNLEN
static inline size_t
strnlen(const char *s, size_t maxlen)
{
    size_t i = 0;
    while((s[i] != '\0') && (i < maxlen)) i++;
    return i;
}
#elif !defined(__USE_GNU)
extern size_t strnlen(const char *s, size_t maxlen);
#endif

#endif /* COMMON_H */
